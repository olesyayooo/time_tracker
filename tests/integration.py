import base64
import hashlib
import hmac
import json
import os
from pathlib import Path
import secrets
import shlex
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
env = dict(os.environ)
# Локально читаем .env, в контейнере настройки приходят через environment.
if (ROOT / '.env').is_file():
    for line in (ROOT / '.env').read_text().splitlines():
        parts = shlex.split(line, comments=True)
        if parts and '=' in parts[0]:
            key, value = parts[0].split('=', 1)
            env.setdefault(key, value)
name = 'tt_api_test_' + secrets.token_hex(6)
binary = env.get('BACKEND_BINARY', str(ROOT / 'build/time_tracker_backend'))
db_mode = env.get('TEST_DB_MODE', 'compose')
if db_mode not in {'direct', 'compose'}:
    raise RuntimeError('TEST_DB_MODE must be direct or compose')

def sql(statement, database='postgres'):
    if db_mode == 'direct':
        # В тестовом контейнере подключаемся к отдельному test-db без Docker-сокета.
        command = ['psql', '-v', 'ON_ERROR_STOP=1', '-At',
                   '-h', env['DB_HOST'], '-p', env.get('DB_PORT', '5432'),
                   '-U', env['POSTGRES_USER'], '-d', database]
        process_env = {**env, 'PGPASSWORD': env['POSTGRES_PASSWORD']}
    else:
        command = ['docker', 'compose', 'exec', '-T', 'db', 'sh', '-c',
                   'exec psql -v ON_ERROR_STOP=1 -At -U "$POSTGRES_USER" -d "$1"', 'sh', database]
        process_env = env
    return subprocess.check_output(command, env=process_env,
        input=statement.encode(), stderr=subprocess.PIPE).decode().strip()

checks = 0
server = None
created = False
log = tempfile.TemporaryFile()
try:
    sql(f'CREATE DATABASE {name};')
    created = True
    sql((ROOT / 'database/init.sql').read_text(), name)
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    env.update(POSTGRES_DB=name, BACKEND_PORT=str(port), BACKEND_HOST='127.0.0.1')
    server = subprocess.Popen([binary], env=env, stdout=log, stderr=log)
    base = f'http://127.0.0.1:{port}'

    def request(method, path, body=None, token=None, expected=200):
        global checks
        headers = {'Content-Type': 'application/json'}
        if token:
            headers['Authorization'] = 'Bearer ' + token
        data = None if body is None else json.dumps(body).encode()
        req = urllib.request.Request(base + path, data=data, headers=headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=15) as response:
                status, raw = response.status, response.read()
        except urllib.error.HTTPError as error:
            status, raw = error.code, error.read()
        assert status == expected, f'{method} {path}: HTTP {status}, expected {expected}; {raw[:200]!r}'
        checks += 1
        return json.loads(raw) if raw else None

    for _ in range(100):
        if server.poll() is not None:
            raise RuntimeError('Test server exited during startup')
        try:
            request('GET', '/health')
            break
        except (OSError, AssertionError):
            time.sleep(0.1)
    else:
        raise RuntimeError('Test server not ready')

    account = {'email': 'Alice@example.com', 'password': 'test-pass-123', 'name': 'Alice'}
    alice = request('POST', '/auth/register', account, expected=201)
    a = alice['access_token']
    assert alice['expires_in'] == 3600 and 'password_hash' not in alice['user']
    request('POST', '/auth/register', {**account, 'email': 'ALICE@EXAMPLE.COM'}, expected=409)
    for email in ['bad', 'a..b@example.com', '.a@example.com', 'a@example', 'a@-example.com']:
        request('POST', '/auth/register', {**account, 'email': email}, expected=400)
    for password in ['short', 'x' * 21]:
        request('POST', '/auth/register', {**account, 'password': password}, expected=400)
    request('POST', '/auth/register', [], expected=400)
    request('POST', '/auth/login', {'email': 'ALICE@example.com', 'password': account['password']})
    request('POST', '/auth/login', {**account, 'password': 'wrong-pass', 'name': 'Alice'}, expected=400)
    request('POST', '/auth/login', {'email': account['email'], 'password': 'wrong-pass'}, expected=401)
    request('POST', '/auth/login', {'email': 'missing@example.com', 'password': 'wrong-pass'}, expected=401)
    assert request('GET', '/users/me', token=a)['id'] == alice['user']['id']
    request('GET', '/users/me', expected=401)
    request('GET', '/users/me', token=a[:-8] + 'abcdefgh', expected=401)

    def b64(value):
        return base64.urlsafe_b64encode(json.dumps(value, separators=(',', ':')).encode()).rstrip(b'=').decode()

    def signed(payload, algorithm='HS256', sign=True):
        message = b64({'alg': algorithm, 'typ': 'JWT'}) + '.' + b64(payload)
        signature = base64.urlsafe_b64encode(hmac.new(env['JWT_SECRET'].encode(), message.encode(), hashlib.sha256).digest()).rstrip(b'=').decode() if sign else ''
        return message + '.' + signature

    now = int(time.time())
    payload = {'iss': 'time-tracker', 'sub': alice['user']['id'], 'iat': now - 7200, 'exp': now - 3600}
    request('GET', '/users/me', token=signed(payload), expected=401)
    payload.update(iat=now, exp=now + 3600)
    request('GET', '/users/me', token=signed(payload, 'none', False), expected=401)
    del payload['exp']
    request('GET', '/users/me', token=signed(payload), expected=401)

    b = request('POST', '/auth/register', {'email': 'bob@example.com', 'password': 'пароль123'}, expected=201)['access_token']
    request('POST', '/projects', {'name': 'Project'}, expected=401)
    for body in [{'name': '   '}, {'name': 'x'*151}, {'name': 123}, {'name': 'x', 'user_id': alice['user']['id']}]:
        request('POST', '/projects', body, token=a, expected=400)
    project = request('POST', '/projects', {'name': '  Учебный проект  '}, token=a, expected=201)
    pid = project['id']
    assert project['name'] == 'Учебный проект' and not project['is_completed']
    assert project['user_id'] == alice['user']['id'] and project['created_at'].endswith('Z')
    path = '/projects/' + pid
    assert len(request('GET', '/projects', token=a)) == 1
    assert request('GET', '/projects', token=b) == []
    request('GET', path, token=a)
    for method, body in [('GET', None), ('PATCH', {'name': 'stolen'}), ('DELETE', None)]:
        request(method, path, body, token=b, expected=404)
    for body in [{}, {'is_completed': 'true'}, {'is_completed': None}, {'name': ''}, {'user_id': alice['user']['id']}]:
        request('PATCH', path, body, token=a, expected=400)
    assert request('PATCH', path, {'is_completed': True}, token=a)['is_completed']
    assert request('PATCH', path, {'name': 'New name'}, token=a)['is_completed']
    request('GET', '/projects/not-a-uuid', token=a, expected=400)
    task_list = f'/projects/{pid}/tasks'
    request('POST', task_list, {'title': 'Task'}, expected=401)
    assert request('GET', task_list, token=a) == []
    request('GET', task_list, token=b, expected=404)
    request('POST', task_list, {'title': 'Stolen'}, token=b, expected=404)
    for body in [{'title': '   '}, {'title': 'x'*201}, {'title': 1}, {'title': 'x', 'project_id': pid}]:
        request('POST', task_list, body, token=a, expected=400)
    task = request('POST', task_list, {'title': '  Учебная задача  '}, token=a, expected=201)
    tid = task['id']
    task_path = '/tasks/' + tid
    assert task['title'] == 'Учебная задача' and task['project_id'] == pid and not task['is_completed'] and not task['is_paused']
    assert len(request('GET', task_list, token=a)) == 1
    request('GET', task_path, token=a)
    for method, body in [('GET', None), ('PATCH', {'title': 'Stolen'}), ('DELETE', None)]:
        request(method, task_path, body, token=b, expected=404)
    for body in [{}, {'title': ''}, {'is_completed': 'true'}, {'project_id': pid}, {'is_paused': True}]:
        request('PATCH', task_path, body, token=a, expected=400)
    assert request('PATCH', task_path, {'is_completed': True}, token=a)['is_completed']
    assert request('PATCH', task_path, {'title': 'New task'}, token=a)['is_completed']
    assert len(request('GET', task_list, token=a)) == 1  # Завершённая задача остаётся в истории.
    request('GET', '/tasks/not-a-uuid', token=a, expected=400)
    request('DELETE', path, token=a, expected=409)  # Нельзя потерять задачи вместе с проектом.
    request('PATCH', task_path, {'is_completed': False}, token=a)
    sql(f"UPDATE tasks SET is_paused=true WHERE id='{tid}';", name)
    assert request('GET', task_path, token=a)['is_paused']
    request('PATCH', task_path, {'is_completed': True}, token=a, expected=409)
    sql(f"UPDATE tasks SET is_paused=false WHERE id='{tid}';", name)
    sql(f"INSERT INTO time_entries (user_id,task_id,started_at) VALUES ('{alice['user']['id']}','{tid}','2026-01-01T12:00:00Z');", name)
    request('PATCH', task_path, {'is_completed': True}, token=a, expected=409)
    def rejected(statement):
        try:
            sql(statement, name)
        except subprocess.CalledProcessError:
            return
        raise AssertionError('Database accepted an invalid timer state')
    rejected(f"UPDATE tasks SET is_completed=true,is_paused=true WHERE id='{tid}';")
    rejected(f"INSERT INTO time_entries (user_id,task_id,started_at) VALUES ('{alice['user']['id']}','{tid}','2026-01-01T13:00:00Z');")
    rejected(f"INSERT INTO time_entries (user_id,task_id,started_at,ended_at) VALUES ('{alice['user']['id']}','{tid}','2026-01-01T13:00:00Z','2026-01-01T14:00:00Z');")
    sql(f"DELETE FROM time_entries WHERE task_id='{tid}';", name)
    rejected(f"INSERT INTO time_entries (user_id,task_id,started_at,ended_at) VALUES ('{alice['user']['id']}','{tid}','2026-01-01T10:00:00Z','2026-01-01T10:00:00Z');")
    sql(f"INSERT INTO time_entries (user_id,task_id,started_at,ended_at) VALUES ('{alice['user']['id']}','{tid}','2026-01-01T10:00:00Z','2026-01-01T11:00:00Z');", name)
    request('DELETE', task_path, token=a, expected=409)
    sql(f"DELETE FROM time_entries WHERE task_id='{tid}';", name)
    request('DELETE', task_path, token=a, expected=204)
    request('GET', task_path, token=a, expected=404)
    request('DELETE', task_path, token=a, expected=404)
    assert request('GET', task_list, token=a) == []
    stored = sql("SELECT password_hash FROM users WHERE email='alice@example.com';", name)
    assert stored.startswith('$argon2id$') and stored != account['password']
    request('DELETE', path, token=a, expected=204)
    request('GET', path, token=a, expected=404)
    request('DELETE', path, token=a, expected=404)
    assert request('GET', '/projects', token=a) == []
    def restart_backend():
        global server
        server.terminate()
        server.wait(timeout=10)
        server = subprocess.Popen([binary], env=env, stdout=log, stderr=log)
        for _ in range(100):
            if server.poll() is not None:
                raise RuntimeError('Backend failed to restart')
            try:
                request('GET', '/health')
                return
            except (OSError, AssertionError):
                time.sleep(0.1)
        raise RuntimeError('Backend did not become ready after restart')
    from timer_scenarios import run
    timer_result = run(request, sql, name, base, a, b, restart_backend)
    print('PASS:', timer_result)
    print(f'PASS: {checks} HTTP checks; JWT validation, Argon2id, email uniqueness, task CRUD, paused state, DB constraints and owner isolation verified.')
except Exception:
    log.seek(0)
    print(log.read().decode(errors='replace')[-6000:])
    raise
finally:
    if server is not None:
        server.terminate()
        try:
            server.wait(timeout=10)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()
    log.close()
    if created:
        sql(f'DROP DATABASE {name};')

import concurrent.futures
import datetime
import json
import threading
import urllib.error
import urllib.parse
import urllib.request


def run(request, sql, database, base, alice, bob, restart):
    def query(path, **values):
        return path + '?' + urllib.parse.urlencode(values)

    def wire(method, path, body=None, token=alice):
        req = urllib.request.Request(base + path, method=method,
            data=None if body is None else json.dumps(body).encode(),
            headers={'Authorization': 'Bearer ' + token, 'Content-Type': 'application/json'})
        try:
            with urllib.request.urlopen(req, timeout=20) as response:
                return response.status, response.headers, response.read()
        except urllib.error.HTTPError as error:
            return error.code, error.headers, error.read()

    project = request('POST', '/projects', {'name': '=SUM(1,2) "Проект"\nстрока'}, token=alice, expected=201)
    pid = project['id']
    tasks = [request('POST', f'/projects/{pid}/tasks', {'title': title}, token=alice, expected=201)
             for title in ['First task', 'Second task']]
    first, second = [task['id'] for task in tasks]
    def control(action, task=first, token=alice, expected=200):
        return request('POST', '/time-entries/' + action, {'task_id': task}, token=token, expected=expected)
    def task_state(task=first):
        return request('GET', '/tasks/' + task, token=alice)
    def history(**filters):
        return request('GET', query('/time-entries', project_id=pid, **filters), token=alice)
    def manual(task, start, end, expected=201, token=alice):
        return request('POST', '/time-entries', {'task_id': task, 'started_at': start, 'ended_at': end}, token=token, expected=expected)

    request('GET', '/time-entries/active', expected=401)
    assert request('GET', '/time-entries/active', token=alice) is None
    assert history() == []
    control('start', token=bob, expected=404)
    control('resume', expected=409)
    control('pause', expected=409)
    control('stop', expected=409)
    for body in [{}, {'task_id': 'bad'}, {'task_id': first, 'user_id': pid}]:
        request('POST', '/time-entries/start', body, token=alice, expected=400)
    started = control('start', first.upper(), expected=201)
    assert started['ended_at'] is None
    active = request('GET', '/time-entries/active', token=alice)
    assert active['id'] == started['id']
    assert request('GET', '/time-entries/active', token=bob) is None
    control('start', expected=409)
    control('start', second, expected=409)
    manual(first, started['started_at'], (datetime.datetime.fromisoformat(started['started_at'].replace('Z', '+00:00')) + datetime.timedelta(microseconds=1)).isoformat(timespec='microseconds'), expected=409)
    control('pause', second, expected=409)
    control('stop', second, expected=409)
    request('PATCH', '/tasks/' + first, {'is_completed': True}, token=alice, expected=409)
    request('PATCH', '/time-entries/' + started['id'], {'ended_at': '2026-01-01T00:00:00Z'}, token=alice, expected=409)
    request('DELETE', '/time-entries/' + started['id'], token=alice, expected=409)
    paused = control('pause')
    assert paused['id'] == started['id'] and paused['ended_at'] is not None
    assert task_state()['is_paused']
    assert request('GET', '/time-entries/active', token=alice) is None
    control('pause', expected=409)
    control('start', expected=409)
    request('PATCH', '/tasks/' + first, {'is_completed': True}, token=alice, expected=409)
    # Another task may run while the first is paused; stopping the first must not stop it.
    running_second = control('start', second, expected=201)
    control('resume', expected=409)
    assert task_state()['is_paused']  # Неудачный Resume не меняет состояние.
    stopped_paused = control('stop')
    assert stopped_paused['time_entry'] is None and not stopped_paused['task']['is_paused']
    assert request('GET', '/time-entries/active', token=alice)['id'] == running_second['id']
    control('pause', second)
    control('start', expected=201)
    control('pause')
    assert task_state(first)['is_paused'] and task_state(second)['is_paused']
    resumed = control('resume', expected=201)
    assert resumed['id'] != started['id'] and not task_state()['is_paused']
    assert resumed['started_at'] > paused['ended_at']
    # Timer survives a complete backend restart; there is no in-memory stopwatch state.
    restart()
    assert request('GET', '/time-entries/active', token=alice)['id'] == resumed['id']
    stopped = control('stop')
    assert stopped['time_entry']['id'] == resumed['id']
    assert not stopped['task']['is_completed'] and not stopped['task']['is_paused']
    control('stop', expected=409)
    control('stop', second)
    request('PATCH', '/tasks/' + first, {'is_completed': True}, token=alice)
    control('start', expected=409)
    control('resume', expected=409)
    manual(first, '2026-01-01T00:00:00Z', '2026-01-01T01:00:00Z', expected=409)
    request('PATCH', '/tasks/' + first, {'is_completed': False}, token=alice)

    # Simultaneous starts on different tasks: exactly one wins, loser gets 409, not 500.
    barrier = threading.Barrier(2)
    def simultaneous(task):
        barrier.wait(timeout=5)
        status, _, body = wire('POST', '/time-entries/start', {'task_id': task})
        return status, json.loads(body)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(simultaneous, [first, second]))
    assert sorted(status for status, _ in results) == [201, 409], results
    winner = next(body['task_id'] for status, body in results if status == 201)
    control('stop', winner)

    # Closed timer intervals can be edited/deleted, and deleting one does not change pause state.
    closed = history()
    assert len(closed) == 5
    for entry in closed:
        request('PATCH', '/time-entries/' + entry['id'], {'ended_at': entry['ended_at']}, token=alice)
        request('DELETE', '/time-entries/' + entry['id'], token=alice, expected=204)
    assert history() == []

    # Deterministic manual history crossing Moscow midnight.
    one = manual(first, '2026-01-01T20:30:00Z', '2026-01-01T21:30:00Z')
    two = manual(first, '2026-01-02T01:00:00+03:00', '2026-01-02T02:00:00+03:00')
    three = manual(second, '2026-01-02T20:30:00Z', '2026-01-02T22:00:00Z')
    assert two['started_at'].startswith('2026-01-01T22:00:00')
    manual(first, '2026-01-01T21:00:00Z', '2026-01-01T22:00:00Z', expected=409)
    manual(second, '2026-01-01T20:45:00Z', '2026-01-01T21:00:00Z', expected=409)
    manual(first, '2026-01-01T21:30:00Z', '2026-01-01T22:00:00Z')  # Adjacent intervals allowed.
    for start, end in [
        ('2026-02-30T00:00:00Z', '2026-03-01T00:00:00Z'),
        ('2026-01-01T00:00:00', '2026-01-01T01:00:00Z'),
        ('2026-01-01T24:00:00Z', '2026-01-02T01:00:00Z'),
        ('2026-01-01T00:00:00+15:00', '2026-01-01T01:00:00Z'),
        ('2026-01-01T00:00:00Z', '2026-01-01T00:00:00Z'),
        ('2026-01-01T02:00:00Z', '2026-01-01T01:00:00Z'),
        ('2099-01-01T00:00:00Z', '2099-01-01T01:00:00Z')]:
        manual(first, start, end, expected=400)
    manual(first, '2026-01-01T00:00:00Z', '2026-01-01T01:00:00Z', token=bob, expected=404)
    for method, body in [('PATCH', {'ended_at': '2026-01-01T21:00:00Z'}), ('DELETE', None)]:
        request(method, '/time-entries/' + one['id'], body, token=bob, expected=404)
    request('PATCH', '/time-entries/' + one['id'], {'ended_at': two['ended_at']}, token=alice, expected=409)
    for body in [{}, {'ended_at': None}, {'task_id': second}, {'user_id': pid}]:
        request('PATCH', '/time-entries/' + one['id'], body, token=alice, expected=400)
    request('PATCH', '/time-entries/' + one['id'], {'started_at': one['started_at'], 'ended_at': one['ended_at']}, token=alice)
    request('GET', query('/time-entries', task_id=first), token=bob, expected=404)
    request('GET', '/time-entries?limit=0', token=alice, expected=400)
    request('GET', '/time-entries?offset=-1', token=alice, expected=400)
    request('GET', '/time-entries?from=invalid', token=alice, expected=400)
    request('GET', '/time-entries?unknown=x', token=alice, expected=400)
    request('GET', query('/time-entries', **{'from': '2026-01-02T00:00:00Z', 'to': '2026-01-01T00:00:00Z'}), token=alice, expected=400)
    assert len(history(limit=2)) == 2
    assert len(history(limit=2, offset=2)) == 2
    assert len(history(task_id=first)) == 3
    assert len(history(**{'from': '2026-01-01T21:30:00Z', 'to': '2026-01-01T22:00:00Z'})) == 1
    assert request('GET', '/time-entries', token=bob) == []
    other_project = request('POST', '/projects', {'name': 'Another project'}, token=alice, expected=201)['id']
    request('GET', query('/time-entries', project_id=other_project, task_id=first), token=alice, expected=400)
    # Editing history remains possible after completion, but adding new entries is forbidden.
    request('PATCH', '/tasks/' + first, {'is_completed': True}, token=alice)
    request('PATCH', '/time-entries/' + one['id'], {'ended_at': one['ended_at']}, token=alice)
    manual(first, '2026-01-01T01:00:00Z', '2026-01-01T02:00:00Z', expected=409)

    params = {'from': '2026-01-01', 'to': '2026-01-03', 'project_id': pid}
    def report(group='project', **changes):
        return request('GET', query('/reports', **{**params, 'group_by': group, **changes}), token=alice)
    project_report = report()
    assert project_report['total_seconds'] == 14400, project_report  # 3600+3600+5400+1800
    assert len(project_report['items']) == 1
    assert project_report['items'][0]['duration_seconds'] == 14400
    task_report = report('task')
    assert sorted(item['duration_seconds'] for item in task_report['items']) == [5400, 9000]
    day_report = report('day')
    assert {item['date']: item['duration_seconds'] for item in day_report['items']} == {
        '2026-01-01': 1800, '2026-01-02': 9000, '2026-01-03': 3600}, day_report
    assert report('day', to='2026-01-01')['total_seconds'] == 1800
    assert report('day', **{'from': '2026-01-02', 'to': '2026-01-02'})['total_seconds'] == 9000
    assert report(task_id=first)['total_seconds'] == 9000
    assert report(**{'from': '2025-01-01', 'to': '2025-01-01'})['items'] == []
    assert request('GET', query('/reports', **{'from': '2026-01-01', 'to': '2026-01-03'}), token=bob)['total_seconds'] == 0
    request('GET', query('/reports', **params), token=bob, expected=404)
    for path in ['/reports', '/reports?from=2026-01-01&to=bad',
                 '/reports?from=2026-02-30&to=2026-03-01',
                 '/reports?from=2026-01-02&to=2026-01-01',
                 '/reports?from=2026-01-01&to=2026-01-02&group_by=bad']:
        request('GET', path, token=alice, expected=400)
    request('GET', '/reports?from=2026-01-01&to=2026-01-02', expected=401)
    # Active work is included only up to the report's single as_of snapshot.
    request('PATCH', '/tasks/' + first, {'is_completed': False}, token=alice)
    active_entry = control('start', expected=201)
    today = (datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(hours=3)).date().isoformat()
    current = report(**{'from': today, 'to': today})
    assert current['as_of'] >= active_entry['started_at'] and len(current['items']) == 1
    control('stop')
    # Cleanup through API proves all created data is owned by this user.
    for entry in history():
        request('DELETE', '/time-entries/' + entry['id'], token=alice, expected=204)
    for task in [first, second]:
        request('DELETE', '/tasks/' + task, token=alice, expected=204)
    request('DELETE', '/projects/' + pid, token=alice, expected=204)
    request('DELETE', '/projects/' + other_project, token=alice, expected=204)
    return 'timer lifecycle/restart/concurrency, manual intervals, timezone reports passed'

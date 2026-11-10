# Сборка Linux-бинарника. Эта стадия не входит в образ для запуска API.
FROM debian:bookworm-slim AS builder
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates \
    libpq-dev libjsoncpp-dev libsodium-dev libssl-dev zlib1g-dev uuid-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY backend/ backend/
RUN cmake -S backend -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    && cmake --build build --parallel 2

# Общие библиотеки для API и тестов. Исходники, компилятор и .env сюда не копируются.
FROM debian:bookworm-slim AS runtime-base
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl libpq5 libjsoncpp25 libsodium23 libssl3 zlib1g libuuid1 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 10001 tracker
WORKDIR /app
COPY --from=builder /src/build/time_tracker_backend /app/time_tracker_backend
RUN chown tracker:tracker /app
USER tracker

# Одноразовый тестовый контейнер. Запускается только по запросу.
FROM runtime-base AS tests
USER root
RUN apt-get update && apt-get install -y --no-install-recommends python3 postgresql-client \
    && rm -rf /var/lib/apt/lists/*
COPY tests/ /app/tests/
COPY database/init.sql /app/database/init.sql
ENV TEST_DB_MODE=direct \
    BACKEND_BINARY=/app/time_tracker_backend \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1
USER tracker
ENTRYPOINT ["python3", "-u", "/app/tests/integration.py"]

# Последняя стадия — обычный API-образ (docker build .).
FROM runtime-base AS api
EXPOSE 8080
ENTRYPOINT ["/app/time_tracker_backend"]

FROM ubuntu:24.04

RUN apt-get update && \
    apt-get install -y \
        build-essential \
        cmake \
        && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --target pastebin -j$(nproc)

EXPOSE 10000

CMD ["./build/pastebin", "--port", "10000"]
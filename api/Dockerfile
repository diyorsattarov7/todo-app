FROM alpine:3.20 AS boost_src
ARG BOOST_VERSION=1.88.0
ARG BOOST_UNDERSCORED_VERSION=${BOOST_VERSION//./_}
RUN apk add --no-cache curl bzip2 ca-certificates
WORKDIR /tmp
RUN curl -fsSL -o boost.tar.bz2 \
      https://archives.boost.io/release/${BOOST_VERSION}/source/boost_${BOOST_UNDERSCORED_VERSION}.tar.bz2 \
 && mkdir -p /src/boost \
 && tar -xjf boost.tar.bz2 -C /src/boost --strip-components=1

FROM alpine:3.20 AS boost_build
RUN apk add --no-cache build-base bash bzip2
WORKDIR /build/boost
COPY --from=boost_src /src/boost/ ./

RUN ./bootstrap.sh \
 && ./b2 -j$(nproc) cxxstd=23 variant=release link=static runtime-link=shared \
        --with-charconv  --with-json \
        --layout=system \
        --prefix=/opt/boost-min \
        install

FROM alpine:3.20 AS builder
RUN apk add --no-cache \
      build-base cmake pkgconf ca-certificates linux-headers \
      openssl-dev
COPY --from=boost_build /opt/boost-min/include /usr/local/include
COPY --from=boost_build /opt/boost-min/lib     /usr/local/lib

WORKDIR /app
COPY . /app

ENV LIBRARY_PATH=/usr/local/lib
ENV LD_LIBRARY_PATH=/usr/local/lib
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --parallel \
 && strip build/api

FROM alpine:3.20 AS runtime
RUN apk add --no-cache ca-certificates libstdc++ openssl
WORKDIR /app
COPY --from=builder /app/build/api /app/api
ENV API_PORT=8080
EXPOSE 8080
CMD ["/app/api"]

FROM golang:1.16-alpine
COPY . /normal_node
WORKDIR /normal_node

RUN export GO111MODULE=on && export GOPROXY=https://mirrors.aliyun.com/goproxy/ && go build -o server ./cmd/server
RUN go build -o client ./cmd/client

RUN chmod +x server
RUN chmod +x client

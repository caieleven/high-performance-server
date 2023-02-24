FROM ubuntu:22.04
RUN sed -i s@/archive.ubuntu.com/@/mirrors.aliyun.com/@g /etc/apt/sources.list && \
sed -i s@/security.ubuntu.com/@/mirrors.aliyun.com/@g /etc/apt/sources.list && \
apt update && \
export DEBIAN_FRONTEND=noninteractive && \
apt install -y telnet net-tools wget g++ &&  \
rm -rf /var/lib/apt/lists/*
CMD ["sh", "-c", "bash"]

FROM fedora:40 AS dev

RUN dnf -y update && \
    dnf -y install git git-lfs cmake ninja-build gcc clang clang-tools-extra llvm gdb python3-pip doxygen && \
    dnf -y install liburing-devel libcurl-devel zip nodejs && \
    pip install cmakelang && \
    dnf clean all

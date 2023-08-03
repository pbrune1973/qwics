FROM ubuntu:latest

RUN apt-get -y update
RUN apt-get -y install build-essential
RUN apt-get -y install autotools-dev
RUN apt-get -y install autoconf
RUN apt-get -y install sudo
RUN apt-get -y install libdb-dev
RUN apt-get -y install db-util
RUN apt-get -y install libgmp3-dev
RUN apt-get -y install libncurses5-dev libncursesw5-dev
RUN apt-get -y install libpq5
RUN apt-get -y install libpq-dev
RUN adduser --disabled-password qwics
RUN usermod -aG sudo qwics

COPY --chown=qwics:qwics ./container/gnucobol-3.1.2 /home/qwics/gnucobol-3.1.2

RUN runuser -l qwics -c 'cd /home/qwics/gnucobol-3.1.2 && ./configure'
RUN runuser -l qwics -c 'cd /home/qwics/gnucobol-3.1.2 && make'
RUN cd /home/qwics/gnucobol-3.1.2 && make install
RUN cd /home/qwics/gnucobol-3.1.2 && ldconfig

COPY --chown=qwics:qwics ./src /home/qwics/src
COPY --chown=qwics:qwics ./cobsrc /home/qwics/cobsrc
COPY --chown=qwics:qwics ./maps /home/qwics/maps
COPY --chown=qwics:qwics ./copybooks /home/qwics/copybooks
COPY --chown=qwics:qwics --chmod=755 ./bin /home/qwics/bin
COPY --chown=qwics:qwics ./LICENSE /home/qwics
COPY --chown=qwics:qwics ./COPYING /home/qwics
COPY --chown=qwics:qwics ./COPYING.LESSER /home/qwics

COPY --chown=qwics:qwics ./container/Makefile /home/qwics
COPY --chown=qwics:qwics ./container/Makefile.batchrun /home/qwics/src/batchrun/Makefile
COPY --chown=qwics:qwics --chmod=755 ./container/entrypoint.sh /home/qwics

RUN runuser -l qwics -c 'cd /home/qwics && make clean; make tpmserver'
RUN runuser -l qwics -c 'cd /home/qwics/src/preps/maps && make clean; make mapprep'
RUN runuser -l qwics -c 'cd /home/qwics/src/preps/cobol && make clean; make cobprep'
RUN runuser -l qwics -c 'cd /home/qwics/src/batchrun && make clean; make batchrun; make jclpars'
RUN runuser -l qwics -c 'mkdir /home/qwics/comm'

CMD runuser -l qwics -c '/home/qwics/entrypoint.sh'

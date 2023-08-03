FROM ibm-semeru-runtimes:open-17-jdk-focal

RUN apt-get -y update
RUN apt-get -y install sudo
RUN adduser --disabled-password qwics
RUN usermod -aG sudo qwics

COPY --chown=qwics:qwics ./QwicsWebApp/target/liberty/wlp /home/qwics/wlp
RUN runuser -l qwics -c 'rm -f /home/qwics/wlp/usr/servers/defaultServer/apps/QwicsWebApp.war.xml'
COPY --chown=qwics:qwics ./QwicsWebApp/target/QwicsWebApp.war /home/qwics/wlp/usr/servers/defaultServer/apps

RUN runuser -l qwics -c 'mkdir /home/qwics/comm'

CMD runuser -l qwics -c 'export JAVA_HOME=/opt/java/openjdk; export PATH=$PATH:/opt/java/openjdk/bin; /home/qwics/wlp/bin/server run'

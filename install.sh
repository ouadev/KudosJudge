#!/bin/sh
rm -Rf /opt/kudosJudge;
mkdir /opt/kudosJudge;
mkdir /opt/kudosJudge/bin;
mkdir /opt/kudosJudge/etc;
mkdir /opt/kudosJudge/tests;

cp bin/kudosd /opt/kudosJudge/bin/kudosd;
cp bin/client /opt/kudosJudge/bin/client;
cp bin/clientloop /opt/kudosJudge/bin/clientloop;
cp bin/syslog /opt/kudosJudge/bin/syslog;
cp bin/stderr /opt/kudosJudge/bin/stderr;
cp bin/clean /opt/kudosJudge/bin/clean;

cp etc/config.ini /opt/kudosJudge/etc/config.ini;
cp -R tests/problems /opt/kudosJudge/tests;

#create symlinks
ln -fs /opt/kudosJudge/bin/kudosd /usr/local/bin/kudosd;
ln -fs /opt/kudosJudge/bin/client /usr/local/bin/kudos-client;
ln -fs /opt/kudosJudge/bin/clientloop /usr/local/bin/kudos-clientloop;
ln -fs /opt/kudosJudge/bin/stderr /usr/local/bin/kudos-stderr;
ln -fs /opt/kudosJudge/bin/syslog /usr/local/bin/kudos-syslog;
ln -fs /opt/kudosJudge/bin/clean  /usr/local/bin/kudos-clean;

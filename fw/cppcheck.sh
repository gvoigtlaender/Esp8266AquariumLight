#!/bin/sh
cppcheck --xml --enable=all --suppress=*:.pio/libdeps/esp12e/PubSubClient/* --suppress=*:.pio/libdeps/esp12e/Syslog/* -DUSE_DISPLAY=0 -DESP8266 --suppress=missingIncludeSystem:* --inline-suppr . 2> cppcheck.xml

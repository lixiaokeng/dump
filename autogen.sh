#!/bin/sh -x
mkdir -p m4
autoreconf -f -i
git checkout -f INSTALL

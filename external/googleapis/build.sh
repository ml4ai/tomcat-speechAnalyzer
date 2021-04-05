#!/usr/bin/env bash

for x in google/cloud/speech/v1/cloud_speech.pb.cc; do
    echo "Building $x"
    make $x
done

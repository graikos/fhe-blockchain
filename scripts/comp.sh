#!/bin/bash

protoc -I=../proto --python_out=. ../proto/message.proto

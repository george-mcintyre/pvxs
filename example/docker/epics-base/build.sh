#!/bin/bash

# Build the Docker image and tag it, login, then push it
docker build --no-cache -t ${DOCKER_USERNAME}/epics-base:latest . && \
docker login ${DOCKER_USERNAME} && \
docker push ${DOCKER_USERNAME}/epics-base:latest

#!/bin/bash

# Build the Docker image and tag it, login, then push it
docker build --build-arg DOCKER_USERNAME=${DOCKER_USERNAME} --pull -t ${DOCKER_USERNAME}/pvxs:latest . && \
docker login && \
docker push ${DOCKER_USERNAME}/pvxs:latest

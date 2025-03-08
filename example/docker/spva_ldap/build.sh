#!/bin/bash

# Build the Docker image and tag it, login, then push it
docker build  --build-arg DOCKER_USERNAME=${DOCKER_USERNAME} --no-cache -t ${DOCKER_USERNAME}/spva_ldap:latest . && \
docker login ${DOCKER_USERNAME} && \
docker push ${DOCKER_USERNAME}/spva_ldap:latest

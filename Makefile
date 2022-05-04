TAG := 1.0

build:
	docker image build -t pintos:${TAG} .
run:
	docker container run --rm -it -v ${PWD}/src:/root/src pintos:${TAG} bash
debug:
	docker container run --rm -it -p 1234:1234 -v ${PWD}/src:/root/src pintos:${TAG} bash

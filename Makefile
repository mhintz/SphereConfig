.PHONY: build run deploy

all: build run

build:
	xcodebuild -configuration Debug -project xcode/SphereConfig.xcodeproj/

run:
	cd ./xcode/build/Debug && ./SphereConfig.app/Contents/MacOS/SphereConfig

deploy: build
	cp -r ./xcode/build/Debug/SphereConfig.app ~/Dropbox/Apps

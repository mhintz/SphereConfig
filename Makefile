.PHONY: build run

all: build run

build:
	xcodebuild -configuration Debug -project xcode/SphereConfig.xcodeproj/

run:
	./xcode/build/Debug/SphereConfig.app/Contents/MacOS/SphereConfig

.PHONY: all build run deploy pull_params

all: build run

build:
	xcodebuild -configuration Debug -project xcode/SphereConfig.xcodeproj/

run:
	cd ./xcode/build/Debug && ./SphereConfig.app/Contents/MacOS/SphereConfig

deploy: build
	cp -r ./xcode/build/Debug/SphereConfig.app ~/Dropbox/Apps

PARAMS_LOCATION := SphereConfig.app/Contents/Resources/savedParams.json

# pulls in the params saved by another copy of the app on another computer...
pull_params:
	cp ~/Dropbox/Apps/$(PARAMS_LOCATION) ./resources/savedParams.json
	cp ~/Dropbox/Apps/$(PARAMS_LOCATION) ./xcode/build/Debug/$(PARAMS_LOCATION)

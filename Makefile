.PHONY: all build run deploy pull_params

all: build run

build:
	xcodebuild -configuration Debug -project xcode/SphereConfig.xcodeproj/

run:
	cd ./xcode/build/Debug && ./SphereConfig.app/Contents/MacOS/SphereConfig

APP_DEPLOY_LOCATION := ~/Dropbox/SphereConfig
PARAMS_LOCATION := SphereConfig.app/Contents/Resources/savedParams.json

deploy: build
	cp -r ./xcode/build/Debug/SphereConfig.app/ $(APP_DEPLOY_LOCATION)/SphereConfig.app/

# pulls in the params saved by another copy of the app on another computer...
pull_params:
	cp $(APP_DEPLOY_LOCATION)/$(PARAMS_LOCATION) ./resources/savedParams.json
	cp $(APP_DEPLOY_LOCATION)/$(PARAMS_LOCATION) ./xcode/build/Debug/$(PARAMS_LOCATION)

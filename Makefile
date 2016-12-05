.PHONY: all build run manual_params_edit deploy pull_params

all: build run

build:
	xcodebuild -configuration Debug -project xcode/SphereConfig.xcodeproj/

run:
	cd ./xcode/build/Debug && ./SphereConfig.app/Contents/MacOS/SphereConfig

APP_LOCAL_FOLDER := ./xcode/build/Debug
APP_DEPLOY_FOLDER := ~/Dropbox/SphereConfig
PARAMS_LOCATION := SphereConfig.app/Contents/Resources/savedParams.json

manual_params_edit:
	cp ./resources/savedParams.json $(APP_LOCAL_FOLDER)/$(PARAMS_LOCATION)

deploy: build
	cp -r $(APP_LOCAL_FOLDER)/SphereConfig.app/ $(APP_DEPLOY_FOLDER)/SphereConfig.app/

# pulls in the params saved by another copy of the app on another computer...
pull_params:
	cp $(APP_DEPLOY_FOLDER)/$(PARAMS_LOCATION) ./resources/savedParams.json
	cp $(APP_DEPLOY_FOLDER)/$(PARAMS_LOCATION) $(APP_LOCAL_FOLDER)/$(PARAMS_LOCATION)

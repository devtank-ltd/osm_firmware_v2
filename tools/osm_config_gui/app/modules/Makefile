THIS_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

JS_DIR:=gui

JS_SRC:=$(shell find $(JS_DIR) -name "*.js")

ESLINT_CFG=.eslintrc.json


default: eslint


.PHONY: eslint

eslint: $(JS_SRC) $(ESLINT_CFG)
	npx eslint -c $(ESLINT_CFG) $(JS_SRC)




#!/bin/bash

git submodule update --remote --merge
git add gtk3/comet_busters gtk3/beatchess
git commit -m "Update submodules to latest main branch"


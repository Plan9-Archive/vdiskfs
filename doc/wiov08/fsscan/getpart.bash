#!/bin/bash

parted -s -m $1 unit B print

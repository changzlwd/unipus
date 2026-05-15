hardware_modules := \
    camera \
    gralloc \
    sensors

# tom_chang_20221029 add, for consumerir.default
hardware_modules += consumerir

include $(call all-named-subdir-makefiles,$(hardware_modules))

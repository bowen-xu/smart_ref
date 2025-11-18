#pragma once

#include <pybind11/pybind11.h>
#include "../smart_ref.hpp"

PYBIND11_DECLARE_HOLDER_TYPE(T, smart_ref::shared_ref<T>);

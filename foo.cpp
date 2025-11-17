#include "smart_ref.hpp"
#include "smart_ref/pybind11.hpp"
#include <iostream>

namespace py = pybind11;
struct Foo
{
    int value;
    Foo(int v) : value(v) { std::cout << "Foo constructor called with value: " << value << std::endl; }
    void greet() { std::cout << "Hello from Foo! Value: " << value << std::endl; }
    ~Foo() { std::cout << "Foo destructor called for value: " << value << std::endl; }
};

using pFoo = smart_ref::shared_ref<Foo>;

static std::vector<pFoo> foo_instances;

#ifdef PYMODULE
PYBIND11_MODULE(foo, m)
{
    m.doc() = R"pbdoc(
        The Mind module
    )pbdoc";
    py::class_<Foo, pFoo>(m, "Foo")
        .def(py::init<int>(), py::arg("value"))
        .def("greet", &Foo::greet, "Greet from Foo")
        .def_readonly("value", &Foo::value);

    m.def(
        "create_foo",
        [](int v, bool cache_instance)
        {
            if (cache_instance){
                foo_instances.emplace_back(new Foo(v));
                return foo_instances.back();
            }
            return pFoo(new Foo(v));
        },
        py::arg("value"), py::arg("cache_instance") = false);
    m.def("equal", [](const Foo &a, const Foo &b) { return a.value == b.value; }, py::arg("a"), py::arg("b"));
    m.def("greet", []() { std::cout << "Hello, Mind!" << std::endl; });
}
#endif // PYMODULE

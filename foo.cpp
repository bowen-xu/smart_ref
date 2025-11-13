#include "smart_ref.hpp"
#include "smart_ref/pybind11.hpp"
#include <iostream>

namespace py = pybind11;
struct Foo
{
    int value;
    Foo(int v) : value(v) {}
    void greet() { std::cout << "Hello from Foo! Value: " << value << std::endl; }
};

using pFoo = smart_ref::shared_ref<Foo>;

#ifdef PYMODULE
PYBIND11_MODULE(foo, m)
{
    m.doc() = R"pbdoc(
        The Mind module
    )pbdoc";
    // py::class_<pFoo>(m, "SharedRefFoo");
    py::class_<Foo, pFoo>(m, "Foo").def(py::init<int>(), py::arg("value")).def("greet", &Foo::greet, "Greet from Foo");

    m.def(
        "create_foo", [](int v) { return pFoo(new Foo(v)); }, py::arg("value"));
    m.def(
        "equal", [](const pFoo &a, const pFoo &b) { return a->value == b->value; }, py::arg("a"), py::arg("b"));
    m.def("greet", []() { std::cout << "Hello, Mind!" << std::endl; });
}
#endif // PYMODULE

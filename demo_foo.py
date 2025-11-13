import foo

print("Calling greet from foo module:")
foo.greet()

a = foo.create_foo(5)
b = foo.create_foo(10)
c = foo.create_foo(5)
print("Are a and b equal?", foo.equal(a, b))
print("Are a and c equal?", foo.equal(a, c))


print("done.")
from clang.cindex import CursorKind
from clang.cindex import Index
from clang.cindex import TypeKind
from nose.tools import raises

kInput = """\

typedef int I;

struct teststruct {
  int a;
  I b;
  long c;
  unsigned long d;
  signed long e;
  const int f;
  int *g;
  int ***h;
};

"""

def get_tu(source=kInput, lang='c'):
    name = 't.c'
    if lang == 'cpp':
        name = 't.cpp'

    index = Index.create()
    tu = index.parse(name, unsaved_files=[(name, source)])
    assert tu is not None
    return tu

def get_cursor(tu, spelling):
    for cursor in tu.cursor.get_children():
        if cursor.spelling == spelling:
            return cursor

    return None

def test_a_struct():
    tu = get_tu(kInput)

    teststruct = get_cursor(tu, 'teststruct')
    assert teststruct is not None, "Could not find teststruct."
    fields = list(teststruct.get_children())
    assert all(x.kind == CursorKind.FIELD_DECL for x in fields)

    assert fields[0].spelling == 'a'
    assert not fields[0].type.is_const_qualified()
    assert fields[0].type.kind == TypeKind.INT
    assert fields[0].type.get_canonical().kind == TypeKind.INT

    assert fields[1].spelling == 'b'
    assert not fields[1].type.is_const_qualified()
    assert fields[1].type.kind == TypeKind.TYPEDEF
    assert fields[1].type.get_canonical().kind == TypeKind.INT
    assert fields[1].type.get_declaration().spelling == 'I'

    assert fields[2].spelling == 'c'
    assert not fields[2].type.is_const_qualified()
    assert fields[2].type.kind == TypeKind.LONG
    assert fields[2].type.get_canonical().kind == TypeKind.LONG

    assert fields[3].spelling == 'd'
    assert not fields[3].type.is_const_qualified()
    assert fields[3].type.kind == TypeKind.ULONG
    assert fields[3].type.get_canonical().kind == TypeKind.ULONG

    assert fields[4].spelling == 'e'
    assert not fields[4].type.is_const_qualified()
    assert fields[4].type.kind == TypeKind.LONG
    assert fields[4].type.get_canonical().kind == TypeKind.LONG

    assert fields[5].spelling == 'f'
    assert fields[5].type.is_const_qualified()
    assert fields[5].type.kind == TypeKind.INT
    assert fields[5].type.get_canonical().kind == TypeKind.INT

    assert fields[6].spelling == 'g'
    assert not fields[6].type.is_const_qualified()
    assert fields[6].type.kind == TypeKind.POINTER
    assert fields[6].type.get_pointee().kind == TypeKind.INT

    assert fields[7].spelling == 'h'
    assert not fields[7].type.is_const_qualified()
    assert fields[7].type.kind == TypeKind.POINTER
    assert fields[7].type.get_pointee().kind == TypeKind.POINTER
    assert fields[7].type.get_pointee().get_pointee().kind == TypeKind.POINTER
    assert fields[7].type.get_pointee().get_pointee().get_pointee().kind == TypeKind.INT

constarrayInput="""
struct teststruct {
  void *A[2];
};
"""
def testConstantArray():
    tu = get_tu(constarrayInput)

    teststruct = get_cursor(tu, 'teststruct')
    assert teststruct is not None, "Didn't find teststruct??"
    fields = list(teststruct.get_children())
    assert fields[0].spelling == 'A'
    assert fields[0].type.kind == TypeKind.CONSTANTARRAY
    assert fields[0].type.get_array_element_type() is not None
    assert fields[0].type.get_array_element_type().kind == TypeKind.POINTER
    assert fields[0].type.get_array_size() == 2

def test_equal():
    """Ensure equivalence operators work on Type."""
    source = 'int a; int b; void *v;'
    tu = get_tu(source)

    a = get_cursor(tu, 'a')
    b = get_cursor(tu, 'b')
    v = get_cursor(tu, 'v')

    assert a is not None
    assert b is not None
    assert v is not None

    assert a.type == b.type
    assert a.type != v.type

    assert a.type != None
    assert a.type != 'foo'

def test_function_argument_types():
    """Ensure that Type.argument_types() works as expected."""
    tu = get_tu('void f(int, int);')
    f = get_cursor(tu, 'f')
    assert f is not None

    args = f.type.argument_types()
    assert args is not None
    assert len(args) == 2

    t0 = args[0]
    assert t0 is not None
    assert t0.kind == TypeKind.INT

    t1 = args[1]
    assert t1 is not None
    assert t1.kind == TypeKind.INT

    args2 = list(args)
    assert len(args2) == 2
    assert t0 == args2[0]
    assert t1 == args2[1]

@raises(TypeError)
def test_argument_types_string_key():
    """Ensure that non-int keys raise a TypeError."""
    tu = get_tu('void f(int, int);')
    f = get_cursor(tu, 'f')
    assert f is not None

    args = f.type.argument_types()
    assert len(args) == 2

    args['foo']

@raises(IndexError)
def test_argument_types_negative_index():
    """Ensure that negative indexes on argument_types Raises an IndexError."""
    tu = get_tu('void f(int, int);')
    f = get_cursor(tu, 'f')
    args = f.type.argument_types()

    args[-1]

@raises(IndexError)
def test_argument_types_overflow_index():
    """Ensure that indexes beyond the length of Type.argument_types() raise."""
    tu = get_tu('void f(int, int);')
    f = get_cursor(tu, 'f')
    args = f.type.argument_types()

    args[2]

@raises(Exception)
def test_argument_types_invalid_type():
    """Ensure that obtaining argument_types on a Type without them raises."""
    tu = get_tu('int i;')
    i = get_cursor(tu, 'i')
    assert i is not None

    i.type.argument_types()

def test_is_pod():
    tu = get_tu('int i; void f();')
    i = get_cursor(tu, 'i')
    f = get_cursor(tu, 'f')

    assert i is not None
    assert f is not None

    assert i.type.is_pod()
    assert not f.type.is_pod()

def test_function_variadic():
    """Ensure Type.is_function_variadic works."""

    source ="""
#include <stdarg.h>

void foo(int a, ...);
void bar(int a, int b);
"""

    tu = get_tu(source)
    foo = get_cursor(tu, 'foo')
    bar = get_cursor(tu, 'bar')

    assert foo is not None
    assert bar is not None

    assert isinstance(foo.type.is_function_variadic(), bool)
    assert foo.type.is_function_variadic()
    assert not bar.type.is_function_variadic()

def test_element_type():
    tu = get_tu('int i[5];')
    i = get_cursor(tu, 'i')
    assert i is not None

    assert i.type.kind == TypeKind.CONSTANTARRAY
    assert i.type.element_type.kind == TypeKind.INT

@raises(Exception)
def test_invalid_element_type():
    """Ensure Type.element_type raises if type doesn't have elements."""
    tu = get_tu('int i;')
    i = get_cursor(tu, 'i')
    assert i is not None
    i.element_type

def test_element_count():
    tu = get_tu('int i[5]; int j;')
    i = get_cursor(tu, 'i')
    j = get_cursor(tu, 'j')

    assert i is not None
    assert j is not None

    assert i.type.element_count == 5

    try:
        j.type.element_count
        assert False
    except:
        assert True

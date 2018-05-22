import unittest

# from test_class_inheritance_pybind11.nested import Pet as Pet_py11
from test_class_inheritance_boost_python.nested import Pet as Pet_bp


# class TestClass(unittest.TestCase):
#
#     def test_name_py11(self):
#         p = Pet_py11('Molly')
#         self.assertEqual(p.name, 'Molly')
#         self.assertEqual(p.getName(), 'Molly')
#
#         p.setName('Charly')
#         self.assertEqual(p.name, 'Charly')
#         self.assertEqual(p.getName(), 'Charly')
#
#     def test_name_pb(self):
#         p = Pet_bp('Molly')
#         self.assertEqual(p.name, 'Molly')
#         self.assertEqual(p.getName(), 'Molly')
#
#         p.setName('Charly')
#         self.assertEqual(p.name, 'Charly')
#         self.assertEqual(p.getName(), 'Charly')
#
#
# if __name__ == '__main__':
#     unittest.main()

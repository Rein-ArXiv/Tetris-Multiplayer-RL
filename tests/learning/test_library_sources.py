"""Reviewed lecture references are independent of Part prose and stay allowlisted."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
import build_learning_library as library

class Sources(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
  self.root=Path(self.temp.name)/'repo';self.root.mkdir()
  self.old=library.ROOT;library.ROOT=self.root;self.addCleanup(setattr,library,'ROOT',self.old)
  (self.root/'python').mkdir();(self.root/'python/model.py').write_text('value = 1\n')
  self.lessons=self.root/'lessons';self.lessons.mkdir()
 def lesson(self,path,state='reviewed'):
  (self.lessons/'1.json').write_text(json.dumps({'state':state,'sections':[{'reference':{'path':path}}]}))
 def test_explicit_reference_without_any_part_mention(self):
  self.lesson('python/model.py')
  self.assertEqual(library.lesson_source_paths(self.lessons),{self.root/'python/model.py'})
  self.lesson('python/missing.py','draft')
  self.assertEqual(library.lesson_source_paths(self.lessons),set())
 def test_reject_non_source_and_missing_paths(self):
  for path in ('python/missing.py','../outside.py','python/.secret.py','accounts.sqlite','/etc/passwd'):
   self.lesson(path)
   with self.assertRaises(ValueError):library.lesson_source_paths(self.lessons)
 def test_reject_symlink_files_and_parent_directories(self):
  outside=Path(self.temp.name)/'outside';outside.mkdir();(outside/'model.py').write_text('private = 1\n')
  (self.root/'python/link.py').symlink_to(outside/'model.py')
  (self.root/'python/alias').symlink_to(outside,target_is_directory=True)
  for path in ('python/link.py','python/alias/model.py'):
   self.lesson(path)
   with self.assertRaises(ValueError):library.lesson_source_paths(self.lessons)

if __name__=='__main__':unittest.main()

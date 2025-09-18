#!/usr/bin/env python3
"""
Tests for custom-tar application
Tests archive creation, listing contents and extraction
"""

import os
import sys
import subprocess
import tempfile
import hashlib
import shutil
from pathlib import Path
import unittest

class CustomTarTest(unittest.TestCase):
    
    def setUp(self):
        """Test environment setup"""
        # Path to executable file
        self.tar_executable = Path(__file__).parent.parent / "build" / "bin" / "custom-tar"
        
        # Check if executable exists
        if not self.tar_executable.exists():
            self.fail(f"Executable not found: {self.tar_executable}")
        
        # Create temporary directories
        self.test_dir = tempfile.mkdtemp()
        self.source_dir = os.path.join(self.test_dir, "source")
        self.extract_dir = os.path.join(self.test_dir, "extracted")
        self.archive_path = os.path.join(self.test_dir, "test.mtar")
        
        os.makedirs(self.source_dir)
        os.makedirs(self.extract_dir)
    
    def tearDown(self):
        """Cleanup after tests"""
        shutil.rmtree(self.test_dir, ignore_errors=True)
    
    def create_test_files(self, base_dir):
        """Creates test file structure"""
        files_created = []
        
        # Create different types of files
        test_files = [
            ("file1.txt", "Hello World!\nThis is a test file.\n"),
            ("file2.bin", bytes(range(256))),  # Binary file
            ("empty.txt", ""),  # Empty file
            ("large.txt", "A" * 10000),  # Larger text file
        ]
        
        for filename, content in test_files:
            file_path = os.path.join(base_dir, filename)
            if isinstance(content, str):
                with open(file_path, 'w') as f:
                    f.write(content)
            else:
                with open(file_path, 'wb') as f:
                    f.write(content)
            files_created.append(file_path)
        
        # Create directories with files
        subdir1 = os.path.join(base_dir, "subdir1")
        subdir2 = os.path.join(base_dir, "subdir2")
        os.makedirs(subdir1)
        os.makedirs(subdir2)
        
        # Files in subdirectories
        sub_files = [
            (os.path.join(subdir1, "sub1.txt"), "Content in subdirectory 1"),
            (os.path.join(subdir1, "sub1.bin"), bytes([1, 2, 3, 4, 5])),
            (os.path.join(subdir2, "sub2.txt"), "Content in subdirectory 2"),
            (os.path.join(subdir2, "duplicate.txt"), "Hello World!\nThis is a test file.\n"),  # Duplicate
        ]
        
        for file_path, content in sub_files:
            if isinstance(content, str):
                with open(file_path, 'w') as f:
                    f.write(content)
            else:
                with open(file_path, 'wb') as f:
                    f.write(content)
            files_created.append(file_path)
        
        return files_created
    
    def run_tar_command(self, command, *args):
        """Runs custom-tar command"""
        cmd = [str(self.tar_executable), command] + list(args)
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            return result.stdout, result.stderr
        except subprocess.CalledProcessError as e:
            self.fail(f"Command {' '.join(cmd)} failed with error:\n"
                     f"stdout: {e.stdout}\n"
                     f"stderr: {e.stderr}\n"
                     f"return code: {e.returncode}")
    
    def calculate_file_hash(self, file_path):
        """Calculates file hash"""
        hash_md5 = hashlib.md5()
        try:
            with open(file_path, "rb") as f:
                for chunk in iter(lambda: f.read(4096), b""):
                    hash_md5.update(chunk)
            return hash_md5.hexdigest()
        except FileNotFoundError:
            return None
    
    def get_directory_structure(self, directory):
        """Returns directory structure and file hashes"""
        structure = {}
        for root, dirs, files in os.walk(directory):
            for file in files:
                file_path = os.path.join(root, file)
                rel_path = os.path.relpath(file_path, directory)
                structure[rel_path] = self.calculate_file_hash(file_path)
        return structure
    
    def test_create_archive(self):
        """Test archive creation"""
        # Create test files
        self.create_test_files(self.source_dir)
        
        # Create archive
        stdout, stderr = self.run_tar_command("create", self.archive_path, self.source_dir)
        
        # Check if archive was created
        self.assertTrue(os.path.exists(self.archive_path), "Archive was not created")
        self.assertGreater(os.path.getsize(self.archive_path), 0, "Archive is empty")
    
    def test_list_archive(self):
        """Test archive content listing"""
        # Create files and archive
        self.create_test_files(self.source_dir)
        self.run_tar_command("create", self.archive_path, self.source_dir)
        
        # List contents
        stdout, stderr = self.run_tar_command("list", self.archive_path)
        
        # Check if expected files are in the listing
        self.assertIn("file1.txt", stdout)
        self.assertIn("file2.bin", stdout)
        self.assertIn("subdir1", stdout)
        self.assertIn("subdir2", stdout)
        self.assertIn("sub1.txt", stdout)
    
    def test_extract_archive(self):
        """Test archive extraction"""
        # Create files and archive
        original_files = self.create_test_files(self.source_dir)
        original_structure = self.get_directory_structure(self.source_dir)
        
        self.run_tar_command("create", self.archive_path, self.source_dir)
        
        # Extract archive
        self.run_tar_command("extract", self.archive_path, self.extract_dir)
        
        # Check if files were extracted
        extracted_structure = self.get_directory_structure(self.extract_dir)
        
        # Compare structures
        self.assertEqual(len(original_structure), len(extracted_structure), 
                        "Number of files doesn't match")
        
        for rel_path, original_hash in original_structure.items():
            self.assertIn(rel_path, extracted_structure, 
                         f"Missing file: {rel_path}")
            self.assertEqual(original_hash, extracted_structure[rel_path], 
                           f"Hash of file {rel_path} doesn't match")
    
    def test_empty_directory(self):
        """Test archiving empty directory"""
        empty_dir = os.path.join(self.test_dir, "empty")
        os.makedirs(empty_dir)
        
        # Create archive from empty directory
        self.run_tar_command("create", self.archive_path, empty_dir)
        
        # Check if archive was created
        self.assertTrue(os.path.exists(self.archive_path))
        
        # Extract and check
        self.run_tar_command("extract", self.archive_path, self.extract_dir)
        
        # Extracted directory should exist, even if empty
        self.assertTrue(os.path.exists(self.extract_dir))
    
    def test_large_files(self):
        """Test archiving large files"""
        # Create large file (1MB)
        large_file = os.path.join(self.source_dir, "large.bin")
        with open(large_file, "wb") as f:
            f.write(b"X" * (1024 * 1024))
        
        original_hash = self.calculate_file_hash(large_file)
        
        # Archive
        self.run_tar_command("create", self.archive_path, self.source_dir)
        
        # Extract
        self.run_tar_command("extract", self.archive_path, self.extract_dir)
        
        # Check hash
        extracted_file = os.path.join(self.extract_dir, "large.bin")
        extracted_hash = self.calculate_file_hash(extracted_file)
        
        self.assertEqual(original_hash, extracted_hash, "Hash of large file doesn't match")
    
    def test_duplicate_files(self):
        """Test handling duplicate files"""
        # Create files with duplicates
        file1 = os.path.join(self.source_dir, "original.txt")
        file2 = os.path.join(self.source_dir, "duplicate.txt")
        
        content = "This content is duplicated"
        with open(file1, 'w') as f:
            f.write(content)
        with open(file2, 'w') as f:
            f.write(content)
        
        # Archive
        self.run_tar_command("create", self.archive_path, self.source_dir)
        
        # Extract
        self.run_tar_command("extract", self.archive_path, self.extract_dir)
        
        # Check if both files exist and have the same content
        extracted1 = os.path.join(self.extract_dir, "original.txt")
        extracted2 = os.path.join(self.extract_dir, "duplicate.txt")
        
        self.assertTrue(os.path.exists(extracted1))
        self.assertTrue(os.path.exists(extracted2))
        
        with open(extracted1, 'r') as f:
            content1 = f.read()
        with open(extracted2, 'r') as f:
            content2 = f.read()
        
        self.assertEqual(content, content1)
        self.assertEqual(content, content2)

if __name__ == "__main__":
    unittest.main()
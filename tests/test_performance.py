#!/usr/bin/env python3
"""
Performance tests and edge cases for custom-tar application
"""

import os
import sys
import subprocess
import tempfile
import hashlib
import shutil
import time
from pathlib import Path
import unittest

class CustomTarPerformanceTest(unittest.TestCase):
    
    def setUp(self):
        """Test environment setup"""
        self.tar_executable = Path(__file__).parent.parent / "build" / "bin" / "custom-tar"
        
        if not self.tar_executable.exists():
            self.fail(f"Executable not found: {self.tar_executable}")
        
        self.test_dir = tempfile.mkdtemp()
        self.source_dir = os.path.join(self.test_dir, "source")
        self.extract_dir = os.path.join(self.test_dir, "extracted")
        self.archive_path = os.path.join(self.test_dir, "test.mtar")
        
        os.makedirs(self.source_dir)
        os.makedirs(self.extract_dir)
    
    def tearDown(self):
        """Cleanup after tests"""
        shutil.rmtree(self.test_dir, ignore_errors=True)
    
    def run_tar_command(self, command, *args):
        """Runs custom-tar command with time measurement"""
        cmd = [str(self.tar_executable), command] + list(args)
        start_time = time.time()
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            end_time = time.time()
            return result.stdout, result.stderr, end_time - start_time
        except subprocess.CalledProcessError as e:
            self.fail(f"Command {' '.join(cmd)} failed with error:\n"
                     f"stdout: {e.stdout}\n"
                     f"stderr: {e.stderr}\n"
                     f"return code: {e.returncode}")
    
    def test_many_small_files(self):
        """Test archiving many small files"""
        print("\n=== Test many small files ===")
        
        # Create 1000 small files
        num_files = 1000
        for i in range(num_files):
            file_path = os.path.join(self.source_dir, f"small_file_{i:04d}.txt")
            with open(file_path, 'w') as f:
                f.write(f"This is small file number {i}\n")
        
        print(f"Created {num_files} small files")
        
        # Archive
        stdout, stderr, create_time = self.run_tar_command("create", self.archive_path, self.source_dir)
        print(f"Archive creation time: {create_time:.2f}s")
        
        # Check archive size
        archive_size = os.path.getsize(self.archive_path)
        print(f"Archive size: {archive_size:,} bytes")
        
        # Extract
        stdout, stderr, extract_time = self.run_tar_command("extract", self.archive_path, self.extract_dir)
        print(f"Extraction time: {extract_time:.2f}s")
        
        # Check if all files were extracted
        extracted_files = list(Path(self.extract_dir).rglob("*.txt"))
        self.assertEqual(len(extracted_files), num_files, 
                        f"Expected {num_files} files, found {len(extracted_files)}")
    
    def test_deep_directory_structure(self):
        """Test deep directory structure"""
        print("\n=== Test deep directory structure ===")
        
        # Create deep directory structure (20 levels)
        current_dir = self.source_dir
        depth = 20
        
        for i in range(depth):
            current_dir = os.path.join(current_dir, f"level_{i:02d}")
            os.makedirs(current_dir)
            
            # Add file at each level
            file_path = os.path.join(current_dir, f"file_at_level_{i}.txt")
            with open(file_path, 'w') as f:
                f.write(f"This file is at directory level {i}\n")
        
        print(f"Created directory structure with depth of {depth} levels")
        
        # Archive
        stdout, stderr, create_time = self.run_tar_command("create", self.archive_path, self.source_dir)
        print(f"Archive creation time: {create_time:.2f}s")
        
        # Extract
        stdout, stderr, extract_time = self.run_tar_command("extract", self.archive_path, self.extract_dir)
        print(f"Extraction time: {extract_time:.2f}s")
        
        # Check if structure was preserved
        extracted_files = list(Path(self.extract_dir).rglob("*.txt"))
        self.assertEqual(len(extracted_files), depth, 
                        f"Expected {depth} files, found {len(extracted_files)}")
    
    def test_special_filenames(self):
        """Test files with special characters in names"""
        print("\n=== Test special filenames ===")
        
        # Files with special characters in names
        special_names = [
            "file with spaces.txt",
            "file-with-dashes.txt", 
            "file_with_underscores.txt",
            "file.with.dots.txt",
            "file(with)parentheses.txt",
            "file[with]brackets.txt",
            "file{with}braces.txt",
            "fileąćęłńóśźż.txt",  # Polish characters
            "file123.txt",
            "FILE_UPPERCASE.TXT",
        ]
        
        created_files = []
        for name in special_names:
            try:
                file_path = os.path.join(self.source_dir, name)
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(f"Content of file: {name}\n")
                created_files.append(name)
            except Exception as e:
                print(f"Failed to create file {name}: {e}")
        
        print(f"Created {len(created_files)} files with special names")
        
        # Archive
        stdout, stderr, create_time = self.run_tar_command("create", self.archive_path, self.source_dir)
        print(f"Archive creation time: {create_time:.2f}s")
        
        # Extract
        stdout, stderr, extract_time = self.run_tar_command("extract", self.archive_path, self.extract_dir)
        print(f"Extraction time: {extract_time:.2f}s")
        
        # Check if all files were extracted
        for name in created_files:
            extracted_path = os.path.join(self.extract_dir, name)
            self.assertTrue(os.path.exists(extracted_path), 
                           f"File {name} was not extracted")
    
    def test_binary_files(self):
        """Test different types of binary files"""
        print("\n=== Test binary files ===")
        
        # Create different types of binary files
        binary_files = [
            ("random.bin", os.urandom(10000)),  # Random data
            ("zeros.bin", b"\x00" * 5000),      # All zeros
            ("ones.bin", b"\xFF" * 5000),       # All ones
            ("pattern.bin", b"\xAA\x55" * 2500), # Pattern
            ("gradient.bin", bytes(range(256)) * 40),  # Gradient
        ]
        
        for filename, data in binary_files:
            file_path = os.path.join(self.source_dir, filename)
            with open(file_path, 'wb') as f:
                f.write(data)
        
        print(f"Created {len(binary_files)} binary files")
        
        # Calculate original hashes
        original_hashes = {}
        for filename, _ in binary_files:
            file_path = os.path.join(self.source_dir, filename)
            with open(file_path, 'rb') as f:
                original_hashes[filename] = hashlib.md5(f.read()).hexdigest()
        
        # Archive
        stdout, stderr, create_time = self.run_tar_command("create", self.archive_path, self.source_dir)
        print(f"Archive creation time: {create_time:.2f}s")
        
        # Extract
        stdout, stderr, extract_time = self.run_tar_command("extract", self.archive_path, self.extract_dir)
        print(f"Extraction time: {extract_time:.2f}s")
        
        # Check integrity
        for filename in original_hashes.keys():
            extracted_path = os.path.join(self.extract_dir, filename)
            with open(extracted_path, 'rb') as f:
                extracted_hash = hashlib.md5(f.read()).hexdigest()
            
            self.assertEqual(original_hashes[filename], extracted_hash,
                           f"Hash of file {filename} doesn't match")
    
    def test_compression_efficiency(self):
        """Test compression/deduplication efficiency"""
        print("\n=== Test deduplication efficiency ===")
        
        # Create many copies of the same file
        base_content = "This is duplicated content. " * 1000
        num_duplicates = 50
        
        for i in range(num_duplicates):
            file_path = os.path.join(self.source_dir, f"duplicate_{i:02d}.txt")
            with open(file_path, 'w') as f:
                f.write(base_content)
        
        # Add some unique files too
        for i in range(5):
            file_path = os.path.join(self.source_dir, f"unique_{i}.txt")
            with open(file_path, 'w') as f:
                f.write(f"This is unique content {i}. " * 100)
        
        # Calculate total source size
        total_source_size = sum(
            os.path.getsize(os.path.join(self.source_dir, f))
            for f in os.listdir(self.source_dir)
        )
        
        print(f"Created {num_duplicates} duplicates + 5 unique files")
        print(f"Total source size: {total_source_size:,} bytes")
        
        # Archive
        stdout, stderr, create_time = self.run_tar_command("create", self.archive_path, self.source_dir)
        archive_size = os.path.getsize(self.archive_path)
        
        print(f"Archive size: {archive_size:,} bytes")
        print(f"Compression ratio: {archive_size / total_source_size * 100:.1f}%")
        
        # Extract
        stdout, stderr, extract_time = self.run_tar_command("extract", self.archive_path, self.extract_dir)
        
        # Check if all files were extracted
        extracted_files = os.listdir(self.extract_dir)
        self.assertEqual(len(extracted_files), num_duplicates + 5,
                        "Incorrect number of extracted files")

if __name__ == "__main__":
    unittest.main(verbosity=2)
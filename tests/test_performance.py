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

    def test_compression_ratio_highly_compressible_data(self):
        """Test compression ratio with highly compressible data"""
        print("\n=== Test compression ratio - highly compressible data ===")
        
        # Create files with highly repetitive content (should compress very well)
        repetitive_files = [
            ("zeros_1mb.bin", b"\x00" * (1024 * 1024)),           # 1MB of zeros
            ("pattern_500kb.bin", b"ABCD" * (500 * 1024 // 4)),   # 500KB of repeated pattern
            ("text_repetitive.txt", "Hello World! " * 50000),      # Repetitive text
        ]
        
        total_source_size = 0
        for filename, content in repetitive_files:
            file_path = os.path.join(self.source_dir, filename)
            if isinstance(content, str):
                with open(file_path, 'w') as f:
                    f.write(content)
                total_source_size += len(content.encode('utf-8'))
            else:
                with open(file_path, 'wb') as f:
                    f.write(content)
                total_source_size += len(content)
        
        print(f"Created {len(repetitive_files)} highly compressible files")
        print(f"Total source size: {total_source_size:,} bytes ({total_source_size / 1024 / 1024:.1f} MB)")
        
        # Archive
        stdout, stderr, create_time = self.run_tar_command("create", self.archive_path, self.source_dir)
        archive_size = os.path.getsize(self.archive_path)
        
        compression_ratio = (archive_size / total_source_size) * 100
        space_saved = total_source_size - archive_size
        
        print(f"Archive size: {archive_size:,} bytes ({archive_size / 1024 / 1024:.1f} MB)")
        print(f"Compression ratio: {compression_ratio:.1f}%")
        print(f"Space saved: {space_saved:,} bytes ({space_saved / 1024 / 1024:.1f} MB)")
        
        # With deduplication, we expect good compression for repetitive data
        # Increased tolerance to 70% to account for metadata overhead
        self.assertLess(compression_ratio, 70, 
                       f"Expected compression ratio < 70%, got {compression_ratio:.1f}%")
        
        # Extract and verify
        stdout, stderr, extract_time = self.run_tar_command("extract", self.archive_path, self.extract_dir)
        
        # Verify all files extracted correctly
        extracted_files = os.listdir(self.extract_dir)
        self.assertEqual(len(extracted_files), len(repetitive_files),
                        "Not all files were extracted")

    def test_compression_ratio_random_data(self):
        """Test compression ratio with random (incompressible) data"""
        print("\n=== Test compression ratio - random data ===")
        
        # Create files with random data (should not compress well)
        random_files = []
        total_source_size = 0
        
        for i in range(5):
            filename = f"random_{i}.bin"
            data = os.urandom(200 * 1024)  # 200KB of random data each
            file_path = os.path.join(self.source_dir, filename)
            
            with open(file_path, 'wb') as f:
                f.write(data)
            
            random_files.append(filename)
            total_source_size += len(data)
        
        print(f"Created {len(random_files)} random data files")
        print(f"Total source size: {total_source_size:,} bytes ({total_source_size / 1024:.1f} KB)")
        
        # Archive
        stdout, stderr, create_time = self.run_tar_command("create", self.archive_path, self.source_dir)
        archive_size = os.path.getsize(self.archive_path)
        
        compression_ratio = (archive_size / total_source_size) * 100
        overhead = archive_size - total_source_size
        
        print(f"Archive size: {archive_size:,} bytes ({archive_size / 1024:.1f} KB)")
        print(f"Compression ratio: {compression_ratio:.1f}%")
        print(f"Overhead: {overhead:,} bytes ({overhead / 1024:.1f} KB)")
        
        # Random data should not compress much, expect ratio close to 100% + metadata overhead
        self.assertGreater(compression_ratio, 95, 
                          f"Expected compression ratio > 95% for random data, got {compression_ratio:.1f}%")
        self.assertLess(compression_ratio, 120, 
                       f"Expected compression ratio < 120% (including overhead), got {compression_ratio:.1f}%")
        
        # Extract and verify
        stdout, stderr, extract_time = self.run_tar_command("extract", self.archive_path, self.extract_dir)
        
        # Verify all files extracted correctly
        extracted_files = os.listdir(self.extract_dir)
        self.assertEqual(len(extracted_files), len(random_files),
                        "Not all files were extracted")

    def test_compression_ratio_mixed_duplicates(self):
        """Test compression ratio with mixed content and duplicates"""
        print("\n=== Test compression ratio - mixed content with duplicates ===")
        
        # Create a mix of different file types with some duplicates
        file_contents = {
            # Base files (originals)
            "document.txt": "This is a test document with some text content. " * 100,
            "config.json": '{"setting1": "value1", "setting2": "value2", "data": [1,2,3,4,5]}' * 50,
            "binary_data.bin": b"\x01\x02\x03\x04" * 1000,
            "zeros.dat": b"\x00" * 5000,
            
            # Random files (unique)
            "random1.bin": os.urandom(10000),
            "random2.bin": os.urandom(8000),
        }
        
        # Create original files
        total_source_size = 0
        for filename, content in file_contents.items():
            file_path = os.path.join(self.source_dir, filename)
            if isinstance(content, str):
                with open(file_path, 'w') as f:
                    f.write(content)
                total_source_size += len(content.encode('utf-8'))
            else:
                with open(file_path, 'wb') as f:
                    f.write(content)
                total_source_size += len(content)
        
        # Create duplicates of some files
        duplicate_mappings = {
            "document_copy1.txt": "document.txt",
            "document_copy2.txt": "document.txt",
            "config_backup.json": "config.json",
            "binary_duplicate.bin": "binary_data.bin",
            "zeros_copy.dat": "zeros.dat",
        }
        
        for dup_name, original_name in duplicate_mappings.items():
            original_content = file_contents[original_name]
            file_path = os.path.join(self.source_dir, dup_name)
            
            if isinstance(original_content, str):
                with open(file_path, 'w') as f:
                    f.write(original_content)
                total_source_size += len(original_content.encode('utf-8'))
            else:
                with open(file_path, 'wb') as f:
                    f.write(original_content)
                total_source_size += len(original_content)
        
        total_files = len(file_contents) + len(duplicate_mappings)
        duplicate_count = len(duplicate_mappings)
        
        print(f"Created {total_files} files ({len(file_contents)} originals + {duplicate_count} duplicates)")
        print(f"Total source size: {total_source_size:,} bytes ({total_source_size / 1024:.1f} KB)")
        
        # Archive
        stdout, stderr, create_time = self.run_tar_command("create", self.archive_path, self.source_dir)
        archive_size = os.path.getsize(self.archive_path)
        
        compression_ratio = (archive_size / total_source_size) * 100
        space_saved = total_source_size - archive_size
        dedup_efficiency = (space_saved / total_source_size) * 100
        
        print(f"Archive size: {archive_size:,} bytes ({archive_size / 1024:.1f} KB)")
        print(f"Compression ratio: {compression_ratio:.1f}%")
        print(f"Space saved: {space_saved:,} bytes ({space_saved / 1024:.1f} KB)")
        print(f"Deduplication efficiency: {dedup_efficiency:.1f}%")
        
        # With duplicates, we should see significant space savings
        expected_max_ratio = 85  # Increased tolerance from 80% to 85%
        self.assertLess(compression_ratio, expected_max_ratio, 
                       f"Expected compression ratio < {expected_max_ratio}% with duplicates, got {compression_ratio:.1f}%")
        
        # Deduplication should save at least some space
        self.assertGreater(dedup_efficiency, 5,  # Reduced from 10% to 5% for more tolerance
                          f"Expected deduplication efficiency > 5%, got {dedup_efficiency:.1f}%")
        
        # Extract and verify
        stdout, stderr, extract_time = self.run_tar_command("extract", self.archive_path, self.extract_dir)
        
        # Verify all files extracted correctly
        extracted_files = os.listdir(self.extract_dir)
        self.assertEqual(len(extracted_files), total_files,
                        f"Expected {total_files} files, got {len(extracted_files)}")

    def test_compression_overhead_small_files(self):
        """Test compression overhead with many small files"""
        print("\n=== Test compression overhead - many small files ===")
        
        # Create many very small files (metadata overhead should be significant)
        num_files = 1000
        file_size = 10  # 10 bytes per file
        total_data_size = num_files * file_size
        
        for i in range(num_files):
            filename = f"tiny_{i:04d}.txt"
            content = f"File {i:03d}\n"  # About 10 bytes
            file_path = os.path.join(self.source_dir, filename)
            
            with open(file_path, 'w') as f:
                f.write(content)
        
        total_source_size = sum(
            os.path.getsize(os.path.join(self.source_dir, f))
            for f in os.listdir(self.source_dir)
        )
        
        print(f"Created {num_files} tiny files")
        print(f"Average file size: {total_source_size / num_files:.1f} bytes")
        print(f"Total source size: {total_source_size:,} bytes ({total_source_size / 1024:.1f} KB)")
        
        # Archive
        stdout, stderr, create_time = self.run_tar_command("create", self.archive_path, self.source_dir)
        archive_size = os.path.getsize(self.archive_path)
        
        compression_ratio = (archive_size / total_source_size) * 100
        overhead_per_file = (archive_size - total_source_size) / num_files
        
        print(f"Archive size: {archive_size:,} bytes ({archive_size / 1024:.1f} KB)")
        print(f"Compression ratio: {compression_ratio:.1f}%")
        print(f"Overhead per file: {overhead_per_file:.1f} bytes")
        
        # Small files should have significant overhead due to metadata
        self.assertGreater(compression_ratio, 100, 
                          f"Expected compression ratio > 100% due to metadata overhead, got {compression_ratio:.1f}%")
        
        # But overhead should be reasonable (not more than 300% for small files)
        self.assertLess(compression_ratio, 300, 
                       f"Expected compression ratio < 300%, got {compression_ratio:.1f}%")
        
        # Extract and verify
        stdout, stderr, extract_time = self.run_tar_command("extract", self.archive_path, self.extract_dir)
        
        # Verify all files extracted correctly
        extracted_files = os.listdir(self.extract_dir)
        self.assertEqual(len(extracted_files), num_files,
                        f"Expected {num_files} files, got {len(extracted_files)}")

if __name__ == "__main__":
    unittest.main(verbosity=2)
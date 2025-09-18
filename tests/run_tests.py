#!/usr/bin/env python3
"""
Script to run all tests for custom-tar application
"""

import unittest
import sys
import os
from pathlib import Path

# Add tests directory to path
sys.path.insert(0, str(Path(__file__).parent))

# Import all test classes
from test_archive import CustomTarTest
from test_performance import CustomTarPerformanceTest

def run_all_tests():
    """Runs all tests"""
    
    # Check if application was built
    tar_executable = Path(__file__).parent.parent / "build" / "bin" / "custom-tar"
    if not tar_executable.exists():
        print("ERROR: custom-tar executable not found")
        print("Please build the project first using: cmake --build build")
        return False
    
    print("=== CUSTOM-TAR APPLICATION TESTS ===\n")
    
    # Create test suite
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add basic tests
    print("Adding basic tests...")
    suite.addTests(loader.loadTestsFromTestCase(CustomTarTest))
    
    # Add performance tests
    print("Adding performance tests...")
    suite.addTests(loader.loadTestsFromTestCase(CustomTarPerformanceTest))
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2, buffer=True)
    result = runner.run(suite)
    
    # Summary
    print(f"\n=== SUMMARY ===")
    print(f"Tests run: {result.testsRun}")
    print(f"Errors: {len(result.errors)}")
    print(f"Failures: {len(result.failures)}")
    
    if result.errors:
        print("\nERRORS:")
        for test, error in result.errors:
            print(f"  - {test}: {error}")
    
    if result.failures:
        print("\nFAILURES:")
        for test, failure in result.failures:
            print(f"  - {test}: {failure}")
    
    success = len(result.errors) == 0 and len(result.failures) == 0
    if success:
        print("\n✅ ALL TESTS PASSED SUCCESSFULLY!")
    else:
        print("\n❌ SOME TESTS FAILED")
    
    return success

if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)
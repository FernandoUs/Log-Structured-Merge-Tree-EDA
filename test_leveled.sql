-- Test script for Leveled LSM with partitioning strategies
-- Run each section separately in the CLI

-- Test 1: Leveled with Size partitioning (Simple comparator)
CREATE TABLE test_size (id INT, location POINT, value DOUBLE) WITH POLICY Leveled 10 COMPARATOR Simple PARTITION Size
INSERT INTO test_size VALUES (0.1, 0.1, 1)
INSERT INTO test_size VALUES (0.5, 0.5, 2)
INSERT INTO test_size VALUES (0.9, 0.9, 3)
SELECT COUNT(*) FROM test_size WHERE spatial_intersect(location, 0, 0, 1, 1)

-- Test 2: Leveled with STR partitioning
CREATE TABLE test_str (id INT, location POINT, value DOUBLE) WITH POLICY Leveled 10 COMPARATOR Simple PARTITION STR
INSERT INTO test_str VALUES (0.2, 0.2, 10)
INSERT INTO test_str VALUES (0.7, 0.7, 20)
INSERT INTO test_str VALUES (0.3, 0.8, 30)
SELECT COUNT(*) FROM test_str WHERE spatial_intersect(location, 0, 0, 1, 1)

-- Test 3: Leveled with R*-groove partitioning + Hilbert comparator
CREATE TABLE test_rstar (id INT, location POINT, value DOUBLE) WITH POLICY Leveled 10 COMPARATOR Hilbert PARTITION RStarGroove
INSERT INTO test_rstar VALUES (0.4, 0.4, 100)
INSERT INTO test_rstar VALUES (0.6, 0.6, 200)
INSERT INTO test_rstar VALUES (0.5, 0.5, 300)
SELECT COUNT(*) FROM test_rstar WHERE spatial_intersect(location, 0, 0, 1, 1)

-- Show all tables
tables

-- Show metrics
metrics

-- Clean up test tables
clean test_size
clean test_str
clean test_rstar

-- Exit
exit

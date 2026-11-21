# Builds and runs the portable code's tests on the host PC (not the GBA).
CXX      ?= g++
CC       ?= gcc
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Werror -I../include -I../third_party/monocypher -I../third_party/qrcodegen
CFLAGS   := -std=c99 -O2

run: totp_host_test vault_host_test store_host_test export_host_test
	./totp_host_test
	./vault_host_test
	./store_host_test
	./export_host_test

totp_host_test: totp_host_test.cpp ../src/totp.cpp ../include/totp.h
	$(CXX) $(CXXFLAGS) totp_host_test.cpp ../src/totp.cpp -o $@

monocypher.o: ../third_party/monocypher/monocypher.c ../third_party/monocypher/monocypher.h
	$(CC) $(CFLAGS) -c ../third_party/monocypher/monocypher.c -o $@

vault_host_test: vault_host_test.cpp ../src/vault.cpp ../include/vault.h monocypher.o
	$(CXX) $(CXXFLAGS) vault_host_test.cpp ../src/vault.cpp monocypher.o -o $@

# The account store with stand-ins for Butano's SRAM, timer and memory (tests/stubs).
store_host_test: store_host_test.cpp ../src/account_store.cpp ../include/account_store.h ../src/vault.cpp \
                 monocypher.o $(wildcard stubs/*.h)
	$(CXX) $(CXXFLAGS) -Istubs store_host_test.cpp ../src/account_store.cpp ../src/vault.cpp monocypher.o -o $@

qrcodegen.o: ../third_party/qrcodegen/qrcodegen.c ../third_party/qrcodegen/qrcodegen.h
	$(CC) $(CFLAGS) -c ../third_party/qrcodegen/qrcodegen.c -o $@

export_host_test: export_host_test.cpp ../src/otp_export.cpp ../include/otp_export.h ../src/totp.cpp qrcodegen.o
	$(CXX) $(CXXFLAGS) export_host_test.cpp ../src/otp_export.cpp ../src/totp.cpp qrcodegen.o -o $@

clean:
	rm -f totp_host_test vault_host_test store_host_test export_host_test monocypher.o qrcodegen.o

.PHONY: run clean

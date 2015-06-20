#!/usr/bin/env ruby

# replace the fentry call at the beginning of each function to nops
# supply objdump command and path to kernel for this to work

# if argument is not 2, show help text
if ARGV.count != 2
  puts "Usage: #{$0} <objdump> <kernel>"
  exit 1
end

OBJDUMP = ARGV[0]
KERNEL = ARGV[1]

# virtual memory addresses and .text segment addresses
TEXT_VMA, TEXT_OFF = `#{OBJDUMP} -h #{KERNEL} | awk '$2 == ".text" { print $4, $6 }'`.split.map(&:hex)
# symbol table
SYMTAB = `#{OBJDUMP} -t #{KERNEL} | awk '$3 == "F" { print $1, $6 }'`

KERNEL_FILE = open(KERNEL, "r+b")

SYMTAB.each_line do |line|
  offset, func = line.split
  offset = offset.hex
  KERNEL_FILE.seek(offset - TEXT_VMA + TEXT_OFF)
  # must check, since there are also some libgcc functions here
  if KERNEL_FILE.read(1).ord == 0xe8
    puts "striping #{func}" if ENV["V"] != "@"
    KERNEL_FILE.seek(offset - TEXT_VMA + TEXT_OFF)
    KERNEL_FILE.write("\x90\x90\x90\x90\x90")
  end
end

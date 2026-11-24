Header				Purpose

include/common/source.h		Source buffers and source locations

include/common/diagnostic.h	Diagnostic reporting interface
include/common/identifier.h	Shared ASCII identifier character rules

include/common/arena.h		Memory arena interface

include/lexer/token.h		Token kinds and token structures

include/lexer/lexer.h		Lexer input and output interface

include/parser/ast.h		Abstract syntax tree structures

include/parser/parser.h		Parser interface

include/sema/sema.h		Semantic analysis interface

include/sema/symbol.h		Symbol and scope interfaces

include/ir/ir.h			Intermediate representation interface

include/codegen/codegen.h	Backend interface
include/codegen/x86_64.h	x86-64 IR translation interface
include/assembler/assembler.h	Experimental ELF object assembler interface

include/driver/options.h	Compiler configuration

include/driver/driver.h		Pipeline orchestration interface

include/string.h		String and byte-memory routine declarations
include/stdio.h			Stream and formatted I/O declarations
include/stdlib.h		Allocation, numeric conversion, and exit declarations
include/stddef.h		Size type and null pointer constant
include/stdint.h		Fixed-width integer types and limits
include/ctype.h			Character classification and case conversion
include/limits.h		Integer width and range constants
include/unistd.h		Descriptor and process routines, plus Crunch file extensions
include/fcntl.h			File-open flags and declarations
include/signal.h		Signal constants and kill declaration
include/poll.h			Poll readiness declarations
include/dirent.h		Linux directory-reading and Crunch decoding declarations
include/sys/types.h		POSIX size and process types
include/sys/stat.h		Linux file metadata and directory declarations
include/sys/utsname.h		Linux system identification declarations
include/sys/wait.h		Child process waiting and status declarations
include/sys/random.h		Linux random-byte syscall declaration
include/sys/socket.h		Linux socket and message declarations
include/sys/uio.h		Socket scatter/gather vectors
include/sys/un.h		Unix-domain socket addresses
include/netinet/in.h		IPv4 socket addresses
include/time.h			Linux nanosleep declarations
include/sys/select.h		Select readiness declarations
include/sys/mman.h		Memory mapping declarations and Crunch failure helper
include/environment.h		Environment-vector lookup declaration
include/test/*.h		Preprocessor integration-test headers

The distinction between src/ and include/ is intentional: implementation details stay in src/, while other modules depend on the declared interfaces in include/.

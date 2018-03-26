# Sharemind assembly language reference

Sharemind assembly files are treated 8-bit ASCII-encoded text files, which may
start with an optional UTF-8 byte-order mark (BOM). Assembly of Sharemind
executables takes place in two stages. First, the text file is scanned into a
stream of tokens. Finally, the stream of tokens is parsed and processed to
assemble sections of a Sharemind executable file.


## Tokenization

We first define some building blocks for the regular expressions used to match
tokens from the input text, and then describe the tokens and how they are
matched.


### Definitions

First, we define a number of character classes for the regular expressions used
later:

| Character class | Equivalent to                | Description            |
|-----------------|------------------------------|------------------------|
| `[:dec:]`       | `0123456789`                 | Decimal digits         |
| `[:hex:]`       | `[:dec:]abcdefABCDEF`        | Hexadecimal digits (both uppercase and lowercase) |
| `[:lower:]`     | `abcdefghijklmnopqrstuvwxyz` | Lowercase letters      |
| `[:upper:]`     | `ABCDEFGHIJKLMNOPQRSTUVWXYZ` | Uppercase letters      |
| `[:alpha:]`     | `[:lower:][:upper:]`         | All letters            |
| `[:alnum:]`     | `[:alpha:][:dec:]`           | All letters and digits |
| `[:ws:]`        | `\ \t\r\v\f`                 | Whitespace, except for newline characters |

Now we define two string classes (or subexpressions) for our regular
expressions. The Kleene star (`*`), the Kleene plus (`+`) and the arity
expression `{m,n}` are considered to be greedy.

| String class    | Equivalent to               |
|-----------------|-----------------------------|
| `(:id:)`        | `[[:alpha:]_][[:alnum:]_]*` |
| `(:hexnumber:)` | `0x[[:hex:]]{1,16}`         |

### Tokens and tokenization process

Tokenization takes place by detecting tokens from the following table in the
order they appear in the table. Most tokens must be matched greedily, so they
are only considered for output to the token stream when fully matched.
`START`, `WHITESPACE`, `COMMENT` and certain `NEWLINE` tokens (see table) are
non-significant and are not passed to the parser.

| Token        | Equivalent to                  | Description                       |
|--------------|--------------------------------|-----------------------------------|
| `START`      | `^(\xEF\xBB\xBF)?`             | Beginning of the text with an optional [byte order mark](https://secure.wikimedia.org/wikipedia/en/wiki/Byte_order_mark). The `START` tokens are not passed to the parser. |
| `WHITESPACE` | `[:ws:]+`                      | Whitespace (of any length) used as a separator. `WHITESPACE` tokens are not passed to the parser. |
| `COMMENT`    | `#[^\n]*`                      | Comment until the end of the line. `COMMENT` tokens are not passed to the parser. |
| `NEWLINE`    | `\n+`                          | End of a line and consecutive empty lines. A `NEWLINE` token before an `EOF` token is not passed to the parser. If the previous token passed to the parser was a `NEWLINE` token, the current `NEWLINE` token is not passed to the parser. |
| `EOF`        | `$`                            | End of text.                      |
| `DIRECTIVE`  | `\.(:id:)`                     | An assembler directive.           |
| `HEX`        | `[\+\-](:hexnumber:)`          | A signed hexadecimal integer (64-bit). |
| `UHEX`       | `(:hexnumber:)`                | An unsigned hexadecimal integer (64-bit). |
| `STRING`     | `\"([^\"]|\\.)*\"`             | A string.                         |
| `LABEL_O`    | `:(:id:)([+\-](:hexnumber:))?` | A label use with a relative offset. |
| `LABEL`      | `:(:id:)`                      | A label definition or use.        |
| `KEYWORD`    | `((:id:)\.)*(:id:)`            | An instruction name or a keyword. |

During tokenization it is considered an error if a `DIRECTIVE`, `HEX`, `UHEX`, `STRING`, `LABEL_O`, `LABEL` or `KEYWORD` token is not followed by a `WHITESPACE`, `COMMENT`, `NEWLINE` or `EOF` token, i.e. in syntax similar to Perl 5 this means `(?=([[:ws:]#\n]|$))`.

## Parsing

Token streams are parsed using the grammar and normalization rules given in the
following subsections.

### Grammar

The following BNF grammar is used to parse the token stream:

```
<program> ::= START <lines> EOF


<lines> ::= <line> NEWLINE <lines>
          | <line>


<line> ::= <directive-line> ; an assembler directive line
         | <code-line>      ; a code statement line
         | LABEL <line>     ; a code or data label
         | LABEL            ; a code or data label


; an assembler directive line consisting of the directive and its
; parameters as described below.
<directive-line> ::= DIRECTIVE <directive-params>


<directive-params> ::= <directive-param> <directive-params>
                     | ε


<directive-param> ::= HEX | UHEX | STRING | KEYWORD


; a code line consisting of the instruction mnemonic and its parameters.
; If the parameters contain other KEYWORD tokens, then the instruction
; mnemonic is formed according to the normalization rule described below.
<code-line> ::= KEYWORD <code-params>


<code-params> ::= <code-param> <code-params>
                | ε


<code-param> ::= HEX | UHEX | LABEL_O | LABEL | KEYWORD
```


## Assembly

The parsed result of the input file contains a list of assembler directives and
code lines. We continue to describe the processing of assembler directives, the
semantics of labels and the translation of code lines.


### Directives

Assembler directives are commands to the assembler about how to proceed. We use
angle brackets to denote parameters and additional regular brackets to denote
`[<optional>]` parameters.


#### `.linking_unit`

`.linking_unit <index>`

| Parameter | Type(s) | Description                    |
|-----------|---------|--------------------------------|
| `<index>` | `UHEX`  | The index of the linking unit. |

Activates the given linking unit for assembly or defines a new linking unit. All
other assembly directives, labels and instructions apply to the active linking
unit. If a Sharemind assembly file writes to a total of _n_ linking units, their
indexes must be _0_, ..., _(n-1)_. There is an artificial limit of 256 linking
units. By default one linking unit with index 0 is defined and active.

Linking units must be defined in order or their indexes, meaning that for all
_n_, the directive `.linking_unit <index>` is valid iff
  * `<index>` is `0x0`, or
  * a linking unit with an index of _(_`<index>` _minus 1)_ has already been
    defined.

**NOTE:** The `.linking_unit` also actives the TEXT section.


#### `.section`

`.section <type>`

| Parameter | Type(s)   | Description          |
|-----------|-----------|----------------------|
| `<type>`  | `KEYWORD` | The type of section. |

Activates the given section in the current linking unit. By default, the active
section is TEXT.

**NOTE:** The active section is set to TEXT also by the `.linking_unit`
directive.


####  `.data`

`.data <type> <value>`

| Parameter | Type(s)                   | Description |
|-----------|---------------------------|-------------|
| `<type>`  | `KEYWORD`                 | Type of value: `uint8`, `uint16`, `uint32`, `uint64`, `int8`, `int16`, `int32`, `int64` or `string`. |
| `<value>` | `HEX`, `UHEX` or `STRING` | Value to write. |

Writes a value to the current section. This directive is not allowed in the
TEXT, BIND or PDBIND sections. For BSS sections, the section is only resized,
and no values are actually written. The number of bytes to write (or, for BSS
sections, to add to the section size) is determined by the type of the value,
or in case of strings, the length of the string given. The length of the string
always contains the terminating NULL byte.


#### `.fill`

`.fill <num> <type> <value>`

| Parameter | Type(s)                   | Description                    |
|-----------|---------------------------|--------------------------------|
| `<num>`   | `UHEX`                    | The number of values to write. |
| `<type>`  | `KEYWORD`                 | Type of value: `uint8`, `uint16`, `uint32`, `uint64`, `int8`, `int16`, `int32`, `int64` or `string`. |
| `<value>` | `HEX`, `UHEX` or `STRING` | Value to write. |

Writes multiple values to the current section. The number of values to write
must be greater than 0 and less than 65536. The semantics of a `.fill` directive
are identical to `<num>` number of `.data` directives with the same `<type>` and
`<value>` arguments.


#### `.bind`

`.bind <signature>`

| Parameter     | Type(s)  | Description |
|---------------|----------|--------------------------------|
| `<signature>` | `STRING` | Name or signature of the system call or protection domain. |

Defines a syscall or protection domain binding to the procedure identified by
the given signature. This directive is only allowed in the BIND and PDBIND
section where it either defines a system call or protection domain binding
respectively.


### Labels

`LABEL` tokens can be used in assembly files to mark positions in the final
output. `LABEL` tokens which are not among the arguments to an assembler
directive or code line, define a label. The name of the label is the value of
the respective `LABEL` token. The value of the label is the offset in the
output of the active section. In case of data sections (DATA, RODATA, BSS) the
value is an offset in bytes. In case of code sections (TEXT) the value is an
offset in the number of 64-bit blocks. In case of the binding sections (BIND and
PDBIND) the value is the number of bindings already output.

`LABEL` tokens as arguments to instructions or directives expand to their 64-bit
unsigned values. The same holds for `LABEL_O` tokens, except that for the final
output the offset given by the `LABEL_O` token is added (or substracted) from
the value.


#### Label constants

Three label constants exist which expand to constant values and can not be
redefined:

| Label     | Expansion |
|-----------|-----------|
| `:RODATA` | `0x1`, the static value of the memory pointer used to access the RODATA section. |
| `:DATA`   | `0x2`, the static value of the memory pointer used to access the DATA section. |
| `:BSS`    | `0x3`, the static value of the memory pointer used to access the BSS section. |


### Instruction code lines

Before an instruction code line (`<code-line>` in the BNF grammar) is assembled,
the assembler applies a normalization process as follows:
  - All values of `KEYWORD` tokens in the respective `<code-params>` list are
    concatenated using underscores between the values, preserving the order of
    the values as they appear in `<code-params>`.
  - The value of the first `KEYWORD` in `<code-line>` is suffixed with an
    underscore and the result of the concatenation in the previous step.
  - All `KEYWORD` tokens are removed from the respective `<code-params>`.

The rationale for this normalization is that all `KEYWORD` tokens in
`<code-params>` are considered to be part of the instruction name specified by
the first `KEYWORD` token in `<code-line>`. Hence the values of all `KEYWORD`
tokens below `<code-line>` are joined together using underscores to form the
actual name of the VM instruction. For example, the following three code lines
are equivalent:

```
mov imm 0x1234 reg 0x2
mov_imm 0x1234 reg 0x2
mov_imm_reg 0x1234 0x2 # Normalized form
```

After normalization code lines consist of an instruction mnemonic followed by a
list of parameters. The mnemonic and the parameters are all translated to their
64-bit equivalents in bytecode and written to the active TEXT section.

**NOTE:** Instruction code lines are only allowed to be used when the active
section is a TEXT section.


## Example
```
.linking_unit 0x0 # No effect, because this is the default

# External procedure binding:
.section BIND
:sys_putc .bind "PutChar::putchar"

# Static read-only data section:
.section RODATA
.data uint64 0x0123456789abcdef # Just some unused data
.fill 0x3 string "la" # "lalala"

# Code section:
.section TEXT
:start
push imm :RODATA            # push the static memory pointer index onto the next call stack
.section RODATA             # Append to RODATA section
:MyString .data string "one\"two\"\"three\"\"\"the end\0"
.section TEXT               # Continue with code
push imm :MyString          # push the offset of the string onto the next call stack
call imm :printZString imm  # call :printZString and discard the return value
halt imm 0x0                # halt the application successfully

# printZString(memory pointer, string offset)
:printZString
# stack[0] is the memory pointer
# stack[1] is the string offset in memory
resizestack 0x3
# stack[2] is now the temporary store for uint8 character.

:printZString_loop
mov imm 0x0 stack 0x2                      # initialize the variable to zero (stack[2] = 0x0)
mov mem_ss 0x0 0x1 stack 0x2 imm 0x1       # move a byte from ptr(stack[0])+stack[1] to &stack[2]
jz imm :printZString_end uint8 stack 0x2   # test for end of zero-terminated string

push stack 0x2                             # push the character to the next call stack
syscall imm :sys_putc imm                  # call putc(uint8)

uinc stack 0x1                             # increment the string offset
jmp imm :printZString_loop                 # loop

:printZString_end
return imm 0x0                             # return from the procedure with the value 0x0
```

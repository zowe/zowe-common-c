# The Zowe Configuration Manager

## Requirements
Zowe is a large, distributed software system and unsurprisingly has a large, distributed configuration.

The Configuation Manager must be

  - Well-defined
  - Verifiable
  - Extensibile
  - Support multiple progamming languages
  - Universal and Singular
  - Evolutionary
  - Backward Compatible
  - Manageable
  - ZOS Deployable


### Well-definedness

The configuration data of Zowe, its base components and extensions must be strongly typed.  For example

  * a TCP Port must be between 1 and 65535
  * a host name must be a valid DNS name or IP address.

This is not to say that there are not parts of the specification are explicitly vague.  Just like in programming,
C provides "void *" is an untyped pointer, TypeScript provides "any".   A system can be strongly typed without being
so specific as to not allow extension or generic operations.

The user value of strong typing is to be able to provide the earliest and most specific feedback to users and support
staff of misconfiguration.   Server failures diagnosed through log files and dumps are NOT the best way to manage
Zowe installation and configuration.

### Verifiable

Zowe not only defines its configuration but enforces syntax and, if possible, semantics.   Many pieces of configuration
data point at ports, key rings, filenames, DNS names, etc.   Often the existence of these resources can be tested as part of installation or as a pre-flight to running a server.  

### Extensible

Zowe will add (and perhaps remove) base components, and provide an ever growing set of third-party extensions.  Just as
the software itself is pluggable the type system of the Zowe's configuration is pluggable.  There are required elements
of a component's configuation that must be specified to be conformant, but room is left to define other specific data
owned by that component.

### Multi-Language Support

Zowe is written in at least C, Java, JavaScript and TypeScript.  The ability to instantiat or access the configuration manager
must be visible to all languages.

### Universal and Singular

The data of the Zowe Configuration must be the single source of truth from all points of view, i.e. servers or the extensions.

### Evolutionary

Zowe components can opt-in to the configuration manager trivially by default and gradually become more strongly typed as they go.
Configuration data and metadata can be compared to aid the forward migration of Zowe installations.

### Backward Compatibility

Not every part of Zowe will use the most evolved means of accessing configuration data at the start of 2.0 LTS.  The semi-standard
of receiving configuration data through environment variables will be supported for

### Manageable

The data and metadata of Zowe configurations is text and can and should be placed under source code control or managed by a configuration management product if compatible.  Users must be able to be as fine-grained as they need to see how their configuration changs.  Modern SCM's such as Git can provide very good managment of stuctured text regardless of syntax.  

## Implementation

### Typing 

Json Schema (https://json-schema.org/)  is the type system of Zowe Configuration, regardless of syntax, specific version 2019 or later.  

### Syntax 

Zowe configuration files are either Yaml, JSON, or a specific Parmlib EBCDIC syntax for ZOS.  Because YAML is (semantically) such a close cousin to JSON there is free interchange between the two provided a few assumptions:

* Unquoted scalars of digits in Yaml are assumed to be JSON integers.
* "null", "NULL", and "Null" are JSON null"
* "true/false", "TRUE/FALSE", and "True/False" are asumed to be JSON booleans.

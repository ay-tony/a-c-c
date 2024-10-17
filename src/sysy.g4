grammar sysy;

program:
    (declaration | functionDefinition)*;

declaration:
    constDeclaration
    | variableDeclaration;

constDeclaration:
    'const' basicType constDefinition (',' constDefinition)* ';';

basicType: 'int' | 'float';

constDefinition:
    IDENTIFIER ('[' expression ']')* '=' constInitializeValue;

constInitializeValue:
    expression
    | '{' (constInitializeValue (',' constInitializeValue)*)? '}';

variableDeclaration:
    basicType variableDefinition (',' variableDefinition)* ';';

variableDefinition:
    IDENTIFIER ('[' expression ']')* ('=' initializeValue)?; 

initializeValue:
    expression
    | '{' (initializeValue (',' initializeValue)*)? '}';

functionDefinition:
    functionType IDENTIFIER '(' functionParameters? ')' block;

functionType: 'void' | 'int' | 'float';

functionParameters:
    functionParameter (',' functionParameter)*;

functionParameter:
    basicType IDENTIFIER ('[' ']' ('[' expression ']')*)?;

block:  
    '{' blockItem* '}';

blockItem:
    declaration
    | statement;

statement:
    leftValue '=' expression ';'                            # assignmentStatement
    | expression? ';'                                       # expressionStatement
    | block                                                 # blockStatement
    | 'if' '(' expression ')' statement ('else' statement)? # ifStatement
    | 'while' '(' expression ')' statement                  # whileStatement
    | 'break' ';'                                           # breakStatement
    | 'continue' ';'                                        # continueStatement
    | 'return' expression? ';'                              # returnStatement;

leftValue:
    IDENTIFIER ('[' expression ']')*;

expression:
    leftValue                                                       # leftValueExpression
    | INTEGER_CONSTANT                                              # integerConstantExpression
    | FLOATING_CONSTANT                                             # floatingConstantExpression
    | IDENTIFIER '(' functionArguments? ')'                         # functionExpression
    | '(' expression ')'                                            # braceExpression
    | (op='+'|op='-'|op='!') expression                             # unaryExpression
    | lhs=expression (op='*'|op='/'|op='%') rhs=expression          # binaryArithmeticExpression
    | lhs=expression (op='+'|op='-') rhs=expression                 # binaryArithmeticExpression
    | lhs=expression op='&&' rhs=expression                         # binaryLogicExpression
    | lhs=expression op='||' rhs=expression                         # binaryLogicExpression
    | lhs=expression (op='<'|op='>'|op='<='|op='>=') rhs=expression # binaryRelationExpression
    | lhs=expression (op='=='|op='!=') rhs=expression               # binaryRelationExpression;

functionArguments:
    expression (',' expression)*;

IDENTIFIER: [a-zA-Z_][0-9a-zA-Z_]*;

FLOATING_CONSTANT: (([0-9]* '.' [0-9]+) | [0-9]+ '.') ([eE] [+-]? [0-9]+)?;
INTEGER_CONSTANT: ([1-9][0-9]*) | ('0' [0-7]*) | ('0' [xX][0-9a-fA-F]*);

WHITESPACE: [ \n\t\r] -> skip;
COMMENT_SINGLE_LINE: '//' .*? '\n' -> skip;
COMMENT_MULTI_LINE: '/*' .*? '*/' -> skip;

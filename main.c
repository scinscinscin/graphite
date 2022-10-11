#include <stdio.h>
#include <scinstdlib.h>
#include <stdlib.h>
#include <string.h>

void print_binary(int number, int width){
	// printf("0b");
	for(int i = 1; i <= width; i++){
		printf("%d", (number >> (width - i)) & 1);
	}
}

struct LexerData{
	char *file_contents;
	int current_line;
	int current_char_index;
	struct Array *tokens;
};

enum TokenType {
	TokenType_STAR,
	TokenType_LBRACE,
	TokenType_RBRACE,
	TokenType_LSQBRACE,
	TokenType_RSQBRACE,
	TokenType_LPAREN,
	TokenType_RPAREN,
	TokenType_SEMICOLON,
	TokenType_COLON,
	TokenType_NONE,
	TokenType_IDENTIFIER,
	TokenType_STRING,
	TokenType_NUMBER,
	TokenType_EOF,
	TokenType_ERROR
};

struct Token{
	enum TokenType type;
	union {
		double number;
		char *string;
		struct {
			int line;
			char ch;
			char *format;
		} error_message;
	} value; 
};

void handle_identifier(char **file_content_ptr, struct LexerData *lexer_data){
	// this function is called when the current character is true when passed through isAlpha();
	char ch;
	struct Array* char_array = CreateArray();
	while(isAlphaNumerical(ch = (*(*file_content_ptr)++))){
		ADD_ELEMENT_TO_ARRAY(char_array, char, ch);
	};

	--(*file_content_ptr);

	char *str = stringify_char_array(char_array);
	struct Token new_identifier_token = {.type = TokenType_IDENTIFIER, .value = {.string = str}};
	ADD_ELEMENT_TO_ARRAY(lexer_data->tokens, struct Token, new_identifier_token);
	FreeArray(char_array);
}

void handle_number_literal(char **file_content_ptr, struct LexerData *lexer_data){
	// this function is called when the current character is true when passed through isNumber();
	char ch;
	struct Array* number_char_array = CreateArray();
	while(isNumber(ch = (*(*file_content_ptr)++))){
		ADD_ELEMENT_TO_ARRAY(number_char_array, char, ch);
	}

	--(*file_content_ptr);

	char *number_literal_as_string = stringify_char_array(number_char_array);
	double number_literal = strtod(number_literal_as_string, NULL);
	free(number_literal_as_string);
	
	struct Token new_number_literal_token = {.type = TokenType_NUMBER, .value = {.number = number_literal}};
	ADD_ELEMENT_TO_ARRAY(lexer_data->tokens, struct Token, new_number_literal_token);
	FreeArray(number_char_array);
}

struct Array *lexer(char *file_contents){
#define HANDLE_SIMPLE_CHAR(token_type) \
	{ \
		struct Token new_token = {.type = token_type, .value = {.number = ch}}; \
		ADD_ELEMENT_TO_ARRAY(tokens, struct Token, new_token); \
		break; \
	}

	struct LexerData *lexer_data = (struct LexerData*) malloc(sizeof(struct LexerData));
	struct Array *tokens = CreateArray();
	lexer_data->tokens = tokens;
	lexer_data->file_contents = file_contents;
	lexer_data->current_line = 0;
	
	char ch;
	while((ch = *(file_contents++)) != 0){
		switch(ch){
			case '\n':
				lexer_data->current_line++;
			case '\t':
			case '\r':
			case ' ':
				break;

			case '*': HANDLE_SIMPLE_CHAR(TokenType_STAR);
			case '{': HANDLE_SIMPLE_CHAR(TokenType_LBRACE);
			case '}': HANDLE_SIMPLE_CHAR(TokenType_RBRACE);
			case '[': HANDLE_SIMPLE_CHAR(TokenType_LSQBRACE);
			case ']': HANDLE_SIMPLE_CHAR(TokenType_RSQBRACE);
			case '(': HANDLE_SIMPLE_CHAR(TokenType_LPAREN);
			case ')': HANDLE_SIMPLE_CHAR(TokenType_RPAREN);
			case ';': HANDLE_SIMPLE_CHAR(TokenType_SEMICOLON);
			case ':': HANDLE_SIMPLE_CHAR(TokenType_COLON);

			default:
				if(isAlpha(ch)){
					--file_contents;
					handle_identifier(&file_contents, lexer_data);	
				}else if(isNumber(ch)){
					--file_contents;
					handle_number_literal(&file_contents, lexer_data);
				}else{
					// Was not able to match the character to a handler
					char *error_message = (char*) malloc(40);
					snprintf(error_message, 40, "Was not able to match character \"%c\"", ch);
					struct Token error_token = {.type = TokenType_ERROR, .value = {.string = error_message}}; 
					ADD_ELEMENT_TO_ARRAY(tokens, struct Token, error_token);
				}
		}
	}
	
	ADD_ELEMENT_TO_ARRAY(tokens, struct Token, (struct Token) {.type = TokenType_EOF})
	free(lexer_data); // TODO: Fix bug
	return tokens;

#undef HANDLE_SIMPLE_CHAR
}

void print_token(struct Token token){
#define HANDLE_SIMPLE_CHAR(ch) \
	case ch: \
		printf("[%s]\n", #ch); \
		break;
	
	switch(token.type){
		HANDLE_SIMPLE_CHAR(TokenType_STAR);
		HANDLE_SIMPLE_CHAR(TokenType_LBRACE);
		HANDLE_SIMPLE_CHAR(TokenType_RBRACE);
		HANDLE_SIMPLE_CHAR(TokenType_LSQBRACE);
		HANDLE_SIMPLE_CHAR(TokenType_RSQBRACE);
		HANDLE_SIMPLE_CHAR(TokenType_LPAREN);
		HANDLE_SIMPLE_CHAR(TokenType_RPAREN);
		HANDLE_SIMPLE_CHAR(TokenType_SEMICOLON);
		HANDLE_SIMPLE_CHAR(TokenType_COLON);
		HANDLE_SIMPLE_CHAR(TokenType_EOF);

		case TokenType_IDENTIFIER:
			printf("[TokenType_IDENTIFIER]: %s\n", token.value.string);
			break;
		case TokenType_NUMBER:
			printf("[TokenType_NUMBER]: %f\n", token.value.number);
			break;
		case TokenType_ERROR:
			printf("[TokenType_ERROR] %s\n", token.value.string);
			break;
		default:
			printf("[print_token]: Was unable to match a token type %d\n", token.type);
			break;
	}

#undef HANDLE_SIMPLE_CHAR
}

void print_tokens(struct Array* tokens){
	printf("=== Printing tokens ===\n");
	for(int i = 0; i < tokens->length; i++){
		printf("[%4d] ", i);
		print_token(GET_ELEMENT_FROM_ARRAY(tokens, struct Token, i));
	}
}

enum CompilerResult {
	CompilerResult_OK,
	CompilerResult_PARSING_ERROR,
	CompilerResult_CODE_GENERATION_ERROR,
};

struct ParsingData{
	struct Array* tokens;
	struct Hashmap* goto_labels;
	int current_token_index;
	int current_generated_line;
	struct Token current_mnemonic;
};

#define GET_CURRENT_TOKEN(p) \
	GET_ELEMENT_FROM_ARRAY(p->tokens, struct Token, p->current_token_index)

struct Token advance(struct ParsingData *parsing_data){
	struct Token return_token = GET_ELEMENT_FROM_ARRAY(parsing_data->tokens, struct Token, parsing_data->current_token_index);
	parsing_data->current_token_index++;
	return return_token;
}

int match(struct ParsingData *parsing_data, enum TokenType token_type){
	if(parsing_data == NULL) return 0;
	struct Token current_token = GET_CURRENT_TOKEN(parsing_data);
	
	if(current_token.type == token_type){
		parsing_data->current_token_index++;
		return 1;
	}

	return 0;
}

int is_finished_parsing_operand(struct Token token){
	int return_value  = token.type == TokenType_SEMICOLON || token.type == TokenType_EOF;
	return return_value;
}

#define OPERAND_DEREFERENCE 1
#define OPERAND_PORT 2
#define OPERAND_IDENTIFIER 4

struct Operand{
	union {
		double number;
		char *string;
	} value;
	uint8_t flags;
};

enum OperandPrecedence{
	OperandPrecedence_NONE,
	OperandPrecedence_PARENTHESIS,
	OperandPrecedence_SQBRACE,
	OperandPrecedence_PRIMARY
};

struct OperandParseTableEntry{
	enum CompilerResult (*handler)(struct ParsingData*, struct Operand*);
	enum OperandPrecedence precedence;	
};

enum CompilerResult paren_operand(struct ParsingData* parsing_data, struct Operand* operand);
enum CompilerResult brace_operand(struct ParsingData* parsing_data, struct Operand* operand);
enum CompilerResult primary_operand(struct ParsingData* parsing_data, struct Operand* operand);
static struct OperandParseTableEntry operand_parse_table[] = {
	[TokenType_LPAREN]     = {paren_operand,   OperandPrecedence_PARENTHESIS},
	[TokenType_LSQBRACE]   = {brace_operand,   OperandPrecedence_SQBRACE},
	[TokenType_IDENTIFIER] = {primary_operand, OperandPrecedence_PRIMARY},
	[TokenType_NUMBER]     = {primary_operand, OperandPrecedence_PRIMARY},
};

enum CompilerResult parse_operand(struct ParsingData *parsing_data, struct Operand* operand, enum OperandPrecedence precedence){
	// take the current token and figure out what to do with it
	struct Token current_token = GET_CURRENT_TOKEN(parsing_data);
	struct OperandParseTableEntry handler = operand_parse_table[current_token.type];

	if(handler.precedence < precedence || handler.handler == NULL){
		printf("[parse_operand] Was not able to find a handler for token type %d\n", current_token.type);
		return CompilerResult_PARSING_ERROR;
	}

	return handler.handler(parsing_data, operand);
}

enum CompilerResult paren_operand(struct ParsingData* parsing_data, struct Operand* operand){
	// consume left parenthesis
	advance(parsing_data);
	operand->flags |= OPERAND_PORT;
	
	// parse the inner value
	enum CompilerResult inner_value_result = 
		parse_operand(parsing_data, operand, operand_parse_table[TokenType_LPAREN].precedence + 1);
	
	if(inner_value_result != CompilerResult_OK) return inner_value_result;
	
	// this should be right parenthesis
	if(!match(parsing_data, TokenType_RPAREN)){
		printf("Expected R_PAREN after inner value of parenthesis expression");
		return CompilerResult_PARSING_ERROR;
	}
	
	return CompilerResult_OK;	
}

enum CompilerResult brace_operand(struct ParsingData* parsing_data, struct Operand* operand){
	// consume left brace
	advance(parsing_data);
	operand->flags |= OPERAND_DEREFERENCE;
	
	// parse the inner value
	enum CompilerResult inner_value_result = 
		parse_operand(parsing_data, operand, operand_parse_table[TokenType_LBRACE].precedence + 1);
	
	if(inner_value_result != CompilerResult_OK) return inner_value_result;

	// this should be right square brace
	if(!match(parsing_data, TokenType_RSQBRACE)){
		printf("Expected RSQ_BRACE after inner value of parenthesis expression");
		return CompilerResult_PARSING_ERROR;
	}

	return CompilerResult_OK;	
}

enum CompilerResult primary_operand(struct ParsingData* parsing_data, struct Operand* operand){
	struct Token current_token = advance(parsing_data);
	switch(current_token.type){
		case TokenType_IDENTIFIER:
			operand->value.string = current_token.value.string;
			operand->flags |= OPERAND_IDENTIFIER;
			break;
		case TokenType_NUMBER:
			operand->value.number = current_token.value.number;
		default: break; // unreachable
	}
	
	return CompilerResult_OK;
}

enum CompilerResult parse_operands(struct ParsingData *parsing_data, struct Array **returned_operands){
	struct Array *operands = CreateArray();
	
	while(!is_finished_parsing_operand(GET_CURRENT_TOKEN(parsing_data))){
		// parse a single operand here
		struct Operand *operand = (struct Operand*) calloc(sizeof(struct Operand), 1);
		enum CompilerResult parse_status = parse_operand(parsing_data, operand, OperandPrecedence_NONE);
		if(parse_status != CompilerResult_OK) return parse_status;
		ADD_ELEMENT_TO_ARRAY(operands, struct Operand, *operand);
	}
	
	match(parsing_data, TokenType_SEMICOLON); // consumes semicolon at the end	
	*returned_operands = operands;
	return CompilerResult_OK;
}

void print_operands(struct Array* operands){
	if(operands == NULL) return;

	printf("=== Printing operands ===\n");
	for(int i = 0; i < operands->length; i++){
		struct Operand current_operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, i);
		
		printf("[%4d] ", i);
		
		if(current_operand.flags & OPERAND_DEREFERENCE) printf("[DEREFERENCED] ");
		if(current_operand.flags & OPERAND_PORT) printf("[PORT] ");
		
		if(current_operand.flags & OPERAND_IDENTIFIER) printf("%s", current_operand.value.string);
		else printf("%f", current_operand.value.number);
		
		printf("\n");
	}
}

int is_operand_immediate(struct Operand operand){ return operand.flags == 0; }
int is_operand_identifier(struct Operand operand){ return operand.flags == OPERAND_IDENTIFIER; }
int is_operand_dereferenced_register(struct Operand operand){ return operand.flags == (OPERAND_IDENTIFIER | OPERAND_DEREFERENCE); }
int is_operand_immediate_memory(struct Operand operand){ return operand.flags == OPERAND_DEREFERENCE; }
int is_operand_immediate_port(struct Operand operand){ return operand.flags == OPERAND_DEREFERENCE; }
int is_operand_dereferenced_port(struct Operand operand){ return operand.flags == (OPERAND_IDENTIFIER | OPERAND_DEREFERENCE | OPERAND_PORT); }

#define REGISTER_IS_GPR 8
#define REGISTER_READABLE_MAIN 4
#define REGISTER_READABLE_SECONDARY 1
#define REGISTER_WRITABLE 2
struct Register{
	const char *name;
	int flags;
};

struct Register graphite_registers[] = {
	[1] = (struct Register) { .name = "ax", .flags = 0b1111 },
	[2] = (struct Register) { .name = "bx", .flags = 0b1111 },
	[3] = (struct Register) { .name = "cx", .flags = 0b1111 },
	[4] = (struct Register) { .name = "dx", .flags = 0b1111 },
	[5] = (struct Register) { .name = "ex", .flags = 0b1111 },
	[6] = (struct Register) { .name = "fx", .flags = 0b1111 },
	[7] = (struct Register) { .name = "gx", .flags = 0b1111 },
};

int get_register_index(const char *const key){
	for(int i = 1; i < sizeof(graphite_registers) / sizeof(struct Register); i++){
		if(strcmp(graphite_registers[i].name, key) == 0){
			return i;
		}
	}
	
	return -1;
}

int verify_register_flags(const char *const key, int flags){
	int reg_index = get_register_index(key);
	if(reg_index != -1){
		if(graphite_registers[reg_index].flags & flags) return reg_index;
	}
	return -1;
}

enum CompilerResult print_opcode_parameters(int opcode_flags, int parameter_1, int parameter_2){
	printf(" ");
	print_binary(opcode_flags, 3);
	printf(" ");
	print_binary(parameter_1, 8);
	printf(" ");
	print_binary(parameter_2, 8);
	printf("\n");
	return CompilerResult_OK;
}

// Right shift can only process numbers sent to the top input bus
// This includes all of the registers, and immediates, immediate memory dereference, immediate io dereference
enum CompilerResult right_shift_handler(struct Array *operands, struct ParsingData *parsing_data){
	struct Operand operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 0);
	
	if(is_operand_identifier(operand)){
		int register_index;
		if((register_index = verify_register_flags(operand.value.string, REGISTER_READABLE_MAIN)) != -1){
			return print_opcode_parameters(0b001, register_index, 0);
		}else{
			printf("Expected register identifier which is readable through main bus. Received %s\n", operand.value.string);
			return CompilerResult_CODE_GENERATION_ERROR;
		}
	}else if(is_operand_immediate(operand)){
		return print_opcode_parameters(0b101, operand.value.number, 0);	
	}else if(is_operand_immediate_memory(operand)){
		return print_opcode_parameters(0b100, 0, operand.value.number);
	}else if(is_operand_immediate_port(operand)){
		return print_opcode_parameters(0b011, 0, operand.value.number);
	}else{
		printf("Expected either register, immediate, immediate memory or port address.\n");
		return CompilerResult_CODE_GENERATION_ERROR;
	}
}

// Negate can process numbers which can be sent to the bottom input bus
// This includes most registers and immediates

enum CompilerResult negation_handler(struct Array *operands, struct ParsingData *parsing_data){
	struct Operand operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 0);

	if(is_operand_identifier(operand)){
		int register_index;
		if((register_index = verify_register_flags(operand.value.string, REGISTER_READABLE_SECONDARY)) != -1){
			return print_opcode_parameters(0b001, 0, register_index);
		}else{
			printf("Expected register identifier which is readable through secondary bus. Received %s\n", operand.value.string);
		}
	}else if(is_operand_immediate(operand)){
		return print_opcode_parameters(0b101, 0, operand.value.number);
	}else{
		printf("Expected either register or immediate.\n");
	}
	
	return CompilerResult_CODE_GENERATION_ERROR;
}

enum CompilerResult arithmetic_handler(struct Array *operands, struct ParsingData *parsing_data){
	if(operands->length < 2){
		printf("Expected atleast two operands for arithmetic binary mnemonic\n");
		return CompilerResult_CODE_GENERATION_ERROR;
	};
	
	struct Operand first_operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 0);
	struct Operand second_operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 1);

	// reg - reg, reg - imm, gpr - imm io, gpr - imm mem, imm - imm
	// Perform recursive descent for the operands
	if(is_operand_immediate(first_operand) && is_operand_immediate(second_operand)){
		return print_opcode_parameters(0b101, first_operand.value.number, second_operand.value.number);
	}

	else if(is_operand_identifier(first_operand)){
		int first_register_index;
		if((first_register_index = get_register_index(first_operand.value.string)) != -1){
			struct Register first_register = graphite_registers[first_register_index];
			
			if(first_register.flags & REGISTER_IS_GPR){
				// check if the second operand is either an imm mem or imm io
				int is_port;
				if((is_port = is_operand_immediate_port(second_operand)) || is_operand_immediate_memory(second_operand)){
					return print_opcode_parameters(is_port ? 0b011 : 0b100, first_register_index, second_operand.value.number);
				}
			}

			if(is_operand_immediate(second_operand)){
				return print_opcode_parameters(0b010, first_register_index, second_operand.value.number);
			}else if(is_operand_identifier(second_operand)){
				int second_register_index;
				if((second_register_index = verify_register_flags(second_operand.value.string, REGISTER_READABLE_SECONDARY)) != -1){
						return print_opcode_parameters(0b001, first_register_index, second_register_index);
				}else{
					printf("Expected second operand to be register label readable using secondary bus. Received %s\n", second_operand.value.string);
				}
			}else{
				printf("Was not able to match a handler for the second operand in arithmetic handler\n");
			}
		}else{
			printf("Expected identifier to contain register label. Received %s\n", first_operand.value.string);
		}
	}else{
		printf("Was not able to perform recursive descent parsing on the operands\n");
	}

	return CompilerResult_CODE_GENERATION_ERROR;
}

enum CompilerResult mov_handler(struct Array *operands, struct ParsingData *parsing_data){
	// Move from register to register
	// Move from register to memory using register dereference
	// Move from register to memory using immediate address
	// Move from register to IO port using immediate address
	// Move to register from memory using register dereference
	// Move to register from memory using immediate address
	// Move to register from IO port using immediate address
	
	if(operands->length < 2){
		printf("Expected atleast two operands for mov command\n");
		return CompilerResult_CODE_GENERATION_ERROR;
	}

	struct Operand first_operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 0);
	struct Operand second_operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 1);

	if(is_operand_identifier(first_operand)){
		int first_register_index;
		if((first_register_index = verify_register_flags(first_operand.value.string, REGISTER_READABLE_MAIN)) != -1){
			if(is_operand_identifier(second_operand)){
				int second_register_index;
				if((second_register_index = verify_register_flags(second_operand.value.string, REGISTER_WRITABLE)) != -1){
					return print_opcode_parameters(0b001, first_register_index, second_register_index);
				}else{
					printf("Expected second operand to contain register label that is writable. Received %s\n", second_operand.value.string);
				}
			}

			else if(is_operand_dereferenced_register(second_operand)){
				int second_register_index;
				if((second_register_index = verify_register_flags(second_operand.value.string, REGISTER_READABLE_SECONDARY)) != -1){
					return print_opcode_parameters(0b010, first_register_index, second_register_index);
				}else{
					printf("Expected second operand to contain register label that is readable through secondary bus. Received %s\n", second_operand.value.string);
				}
			}
			
			else if(is_operand_immediate_memory(second_operand)){
				return print_opcode_parameters(0b011, first_register_index, second_operand.value.number);
			}
			
			else if(is_operand_immediate_port(second_operand)){
				return print_opcode_parameters(0b100, first_register_index, second_operand.value.number);
			}
		}else{
			printf("Expected first operand to contain register label readable through the main bus. Received %s\n", first_operand.value.string);
		}
	}else if(is_operand_identifier(second_operand)){
		int second_register_index;
		if((second_register_index = verify_register_flags(second_operand.value.string, REGISTER_WRITABLE)) != -1){
			if(is_operand_dereferenced_register(first_operand)){
				int first_register_index;
				if((first_register_index = verify_register_flags(first_operand.value.string, REGISTER_READABLE_SECONDARY)) != -1){
					return print_opcode_parameters(0b101, first_register_index, second_register_index);
				}else{
					printf("Expected first operand to contain register label that is readable through secondary bus. Received %s\n", first_operand.value.string);
				}
			}else if(is_operand_immediate_memory(first_operand)){
				return print_opcode_parameters(0b110, first_operand.value.number, second_register_index);
			}else if(is_operand_immediate_port(first_operand)){
				return print_opcode_parameters(0b111, first_operand.value.number, second_register_index);
			}else{
				printf("Expected first operand to either be register dereference, immediate memory address, or immediate port address\n");
			}
		}
	}else{
		printf("Was not able to perform recursive descent on the operands\n");
	}

	return CompilerResult_CODE_GENERATION_ERROR;
}

enum CompilerResult loadimm_handler(struct Array *operands, struct ParsingData *parsing_data){
	// Load immediate to register
	// Load immediate to memory using register deref
	// Load immediate to memory using immediate address
	// Load immediate to port using immediate address
	// Load immediate to port using register deref
	if(operands->length < 2){
		printf("Expected atleast two operands for loadimm mnemonic\n");
		return CompilerResult_CODE_GENERATION_ERROR;
	}

	struct Operand first_operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 0);
	struct Operand second_operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 1);

	if(is_operand_immediate(first_operand)){
		if(second_operand.flags & OPERAND_IDENTIFIER){
			int register_index;
			if(second_operand.flags & OPERAND_DEREFERENCE){
				if((register_index = verify_register_flags(second_operand.value.string, REGISTER_READABLE_SECONDARY)) != -1){
					return print_opcode_parameters(second_operand.flags & OPERAND_PORT ? 0b101 : 0b010, first_operand.value.number, register_index);
				}else{
					printf("Expected second operand to be register label readable through secondary bus. Received %s\n", second_operand.value.string);
				}
			}else{
				if((register_index = verify_register_flags(second_operand.value.string, REGISTER_WRITABLE)) != -1){
					return print_opcode_parameters(0b001, first_operand.value.number, register_index);
				}else{
					printf("Expected second operand to be register label that is writable. Received %s\n", second_operand.value.string);
				}
			}
		}else if(second_operand.flags & OPERAND_DEREFERENCE || second_operand.flags & OPERAND_PORT){
			return print_opcode_parameters(second_operand.flags & OPERAND_PORT ? 0b100 : 0b011, first_operand.value.number, second_operand.value.number);
		}else{
			printf("Expected second parameter to either be register or dereferenced immediate\n");
		}
	}else{
		printf("Expected first operand to be an immediate.\n");
	}

	return CompilerResult_CODE_GENERATION_ERROR;
}

enum CompilerResult push_handler(struct Array *operands, struct ParsingData *parsing_data){
	// Push register into stack
	// Push immediate into stack
	if(operands->length < 1){
		printf("Expected atleast one operand for push mnemonic.\n");
		return CompilerResult_CODE_GENERATION_ERROR;
	}

	struct Operand operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 0);
	if(is_operand_identifier(operand)){
		int register_index;
		if((register_index = verify_register_flags(operand.value.string, REGISTER_READABLE_MAIN)) != -1){
			print_opcode_parameters(0b001, register_index, 0);
		}else{
			printf("Expected parameter to be register label that is readable through the main bus. Received %s\n", operand.value.string);
		}
	}else if(is_operand_immediate(operand)){
		print_opcode_parameters(0b010, operand.value.number, 0);
	}else{
		printf("Expected push parameter to be either register or immediate\n");
	}

	return CompilerResult_CODE_GENERATION_ERROR;
}

enum CompilerResult pop_handler(struct Array *operands, struct ParsingData *parsing_data){
	if(operands->length < 1){
		printf("Expected atleast one operand for pop mnemonic.\n");
		return CompilerResult_CODE_GENERATION_ERROR;
	}

	struct Operand operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 0);
	if(is_operand_identifier(operand)){
		int register_index;
		if((register_index = verify_register_flags(operand.value.string, REGISTER_IS_GPR)) != -1){
			print_opcode_parameters(0b001, register_index, 0);	
		}else{
			printf("Expected parameter to be general purpose register label\n");
		}
	}else{
		printf("Expected operand to be identifie.\n");
	}
	return CompilerResult_CODE_GENERATION_ERROR;
}

char *reset_targets[] = { NULL, "gpr", "mem", "stack", "io", "acc", "flag" };
enum CompilerResult reset_handler(struct Array *operands, struct ParsingData *parsing_data){
	if(operands->length < 1){
		printf("Expected atleast 1 operand for reset mnemonic.\n");
		return CompilerResult_CODE_GENERATION_ERROR;
	}
	
	struct Operand operand = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 0);
	if(is_operand_identifier(operand)){
		// determine the index of the target
		for(int i = 1; i < (sizeof(reset_targets) / sizeof(char*)); i++){
			if(strcmp(reset_targets[i], operand.value.string) == 0){
				return print_opcode_parameters(i, 0, 0);
			}
		}

		printf("Did not find reset target with the label of %s\n", operand.value.string);
	}else{
		printf("Expected operand to for reset mnemonic be identifier\n");
	}
	return CompilerResult_CODE_GENERATION_ERROR;
}

enum CompilerResult nop_handler(struct Array *operands, struct ParsingData *parsing_data){
	return print_opcode_parameters(0, 0, 0);
}

enum CompilerResult jmp_handler(struct Array *operands, struct ParsingData *parsing_data){
	// Jump immediate
	// Jump from register
	// Jump from immediate memory address
	// Jump from dereferenced memory address
	// Jump from immediate IO address
	// Jump from dereferenced IO address
	
	int is_conditional = strcmp(parsing_data->current_mnemonic.value.string, "cjmp") == 0;
	if(operands->length < (is_conditional ? 2 : 1)){
		printf("Expected atleast %d operands for %s mnemonic.\n", is_conditional ? 2 : 1, parsing_data->current_mnemonic.value.string);
		return CompilerResult_CODE_GENERATION_ERROR;
	}

	struct Operand target = GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 0);
	int flags = is_conditional ? (GET_ELEMENT_FROM_ARRAY(operands, struct Operand, 1)).value.number : 0;
	
	if(target.flags & OPERAND_IDENTIFIER){
		int register_index;
		if((register_index = get_register_index(target.value.string)) != -1){
			if(target.flags & OPERAND_DEREFERENCE){
				if(verify_register_flags(target.value.string, REGISTER_READABLE_SECONDARY)){
					return print_opcode_parameters(target.flags & OPERAND_PORT ? 0b110 : 0b100, register_index, flags);
				}else{
					printf("Expected register to be readable through secondary bus\n");
				}
			}else{
				if(verify_register_flags(target.value.string, REGISTER_READABLE_MAIN)){
					return print_opcode_parameters(0b010, register_index, flags);
				}else{
					printf("Expected register to be readable through main bus\n");
				}	
			}
		}else{
			// check if it's a goto label
			if(isKeyInHashmap(parsing_data->goto_labels, target.value.string) == 1){
				int jump_point = GET_ELEMENT_FROM_HASHMAP(parsing_data->goto_labels, target.value.string, int);
				print_opcode_parameters(0b001, jump_point, flags);
			}else{
				printf("Was not able to match identifier in goto parameter to either register or goto label");
			}
		}
	}else{
		if(target.flags & OPERAND_DEREFERENCE){
			return print_opcode_parameters(target.flags & OPERAND_PORT ? 0b101 : 0b011, target.value.number, flags);
		}else{
			return print_opcode_parameters(0b001, target.value.number, flags);
		}
	}
	
	return CompilerResult_CODE_GENERATION_ERROR;
}

struct Mnemonic{
	const char *const name;
	enum CompilerResult (*operand_handler)(struct Array *operands, struct ParsingData *parsing_data);
};

struct Mnemonic graphite_mnemonics[] = {
	[0b00000] = { .name = "nop",      .operand_handler = &nop_handler  },
	[0b00001] = { .name = "add",      .operand_handler = &arithmetic_handler  },
	[0b00010] = { .name = "sub",      .operand_handler = &arithmetic_handler  },
	[0b00011] = { .name = "xor",      .operand_handler = &arithmetic_handler  },
	[0b00100] = { .name = "and",      .operand_handler = &arithmetic_handler  },
	[0b00101] = { .name = "or",       .operand_handler = &arithmetic_handler  },
	[0b00110] = { .name = "xnor",     .operand_handler = &arithmetic_handler  },
	[0b00111] = { .name = "nand",     .operand_handler = &arithmetic_handler  },
	[0b01000] = { .name = "nor",      .operand_handler = &arithmetic_handler  },
	[0b01001] = { .name = "rs",       .operand_handler = &right_shift_handler },
	[0b01010] = { .name = "neg",      .operand_handler = &negation_handler    },
	[0b10001] = { .name = "mov",      .operand_handler = &mov_handler         },
	[0b10010] = { .name = "loadimm",  .operand_handler = &loadimm_handler     },
	[0b10011] = { .name = "push",     .operand_handler = &push_handler        },
	[0b10100] = { .name = "pop",      .operand_handler = &pop_handler         },
	[0b10101] = { .name = "reset",    .operand_handler = &reset_handler       },
	[0b10110] = { .name = "resetall", .operand_handler = &nop_handler         },
	[0b11101] = { .name = "jmp",      .operand_handler = &jmp_handler         },
	[0b11110] = { .name = "cjmp",     .operand_handler = &jmp_handler         },
	[0b11111] = { .name = "hlt",      .operand_handler = &nop_handler         },
};

int find_index_of_mnemonic(const char *const key){
	for(int i = 0; i < sizeof(graphite_mnemonics) / sizeof(struct Mnemonic); i++){
		if(graphite_mnemonics[i].name == NULL) continue;
		if(strcmp(graphite_mnemonics[i].name, key) == 0) return i;
	}
	return -1;
}

enum CompilerResult parse_token(struct ParsingData* parsing_data, struct Token first_token){
	switch(first_token.type){
		case TokenType_IDENTIFIER:{
			int identifier_handler_index = find_index_of_mnemonic(first_token.value.string);
			struct Token mnemonic_token = advance(parsing_data);
			parsing_data->current_mnemonic = mnemonic_token;
			
			if(identifier_handler_index != -1){
				struct Mnemonic identifier_handler = graphite_mnemonics[identifier_handler_index];
				struct Array *operands;
				enum CompilerResult operand_parsing_status = parse_operands(parsing_data, &operands);
				if(operand_parsing_status != CompilerResult_OK) return operand_parsing_status;
				// print_operands(operands);
				
				print_binary(identifier_handler_index, 5);
				enum CompilerResult parsing_status = identifier_handler.operand_handler(operands, parsing_data);
				return parsing_status;
			}

			else{
				if(!match(parsing_data, TokenType_COLON)){
					printf("Identifier %s is not a mnemonic nor is it a goto label. Expected TokenType_COLON, Received: %d\n", first_token.value.string, (GET_CURRENT_TOKEN(parsing_data)).type);
					return CompilerResult_PARSING_ERROR;
				}

				// add goto label here
				// printf("Adding goto label %s pointing to index %d\n", first_token.value.string, parsing_data->current_generated_line);
				ADD_ELEMENT_TO_HASHMAP(parsing_data->goto_labels, first_token.value.string, int, parsing_data->current_generated_line);
				return CompilerResult_OK;
			}
			
			break;
		}
		
		default:
			printf("Was not able to find a handler for token type: %d\n", first_token.type);
			return CompilerResult_PARSING_ERROR;
	}
}

enum CompilerResult parse(struct Array* tokens){
	struct ParsingData* parsing_data = (struct ParsingData*) malloc(sizeof(struct ParsingData));
	parsing_data->tokens = tokens;
	parsing_data->current_generated_line = 0;
	parsing_data->current_token_index = 0;
	parsing_data->goto_labels = CreateHashmap();
	
	struct Token current_token;
	while((current_token = GET_CURRENT_TOKEN(parsing_data)).type != TokenType_EOF){
		enum CompilerResult result = parse_token(parsing_data, current_token);
		if(result != CompilerResult_OK) return result;
		
		parsing_data->current_generated_line++;
		// break; // remove this later, right now it only generates code for one line
	}
	
	return CompilerResult_OK;
}

int main(int argc, char **argv){
	if(argc < 2){
		printf("Expected atleast one argument\n");
		return -1;
	}
	
	char *file_contents = readFile(argv[1]);
	struct Array *tokens = lexer(file_contents);
	free(file_contents);
	// print_tokens(tokens);

	struct Array *before_tampered = tokens;
	parse(tokens);

	FreeArray(tokens);
	return 0;
}

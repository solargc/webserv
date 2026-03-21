NAME		= webserv

CC			= c++
CFLAGS		= -Werror -Wextra -Wall -std=c++98

SRC_DIR		= src
OBJ_DIR		= obj
INC_DIR		= includes

LIB_SRCS	= Config/Config.cpp \
			  Config/tokenizer.cpp \
			  Config/parser.cpp \
			  Server.cpp

SRCS		= main.cpp $(LIB_SRCS)

OBJS		= $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

test:
	-$(CC) $(CFLAGS) -I$(INC_DIR) $(addprefix $(SRC_DIR)/, $(LIB_SRCS)) tests/config_test.cpp -o tests/config_test
	-$(CC) $(CFLAGS) -I$(INC_DIR) $(addprefix $(SRC_DIR)/, $(LIB_SRCS)) tests/server_test.cpp -o tests/server_test
	-./tests/config_test
	-./tests/server_test

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME) tests/config_test tests/server_test

re: fclean all

.PHONY: all clean fclean re test

#include "../tree.h"

int main() {
	create_node("test");
	create_node("test2");
	create_node("bruh");
	create_node("1");
	create_node("2");
	create_node("3");
	create_node("aaaaaaaaaaaaaaa");

	print_tree();

	clean_tree();

	create_node("teeest");
	create_node("letzter test");

	print_tree();
}

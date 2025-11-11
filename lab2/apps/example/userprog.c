#include "userprog.h"

// main process where the string is passed as an argument to prodcuers
void main(){
    // ask for the input string
    char input_string[100];
    Printf("Enter a string: ");
    // read the input string
    int i = 0;
    char ch;
    while((ch = getchar()) != '\n' && i < 99){
        input_string[i++] = ch;

    }
    input_string[i] = '\0'; // null-terminate the string
    Printf("You entered: %s\n", input_string);
    // let producers consume each character of the string and send it to buffer
    for(int j = 0; j < i; j++){
        // call producer function with each character
        producer(input_string[j]);
    }

    Exit();
}

void producer(char item){
    // produce the item (character)
    Printf("Producing item: %c\n", item);
    // 
}
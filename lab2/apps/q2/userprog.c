#include "userprog.h"
#include "synch.h"


// initializing the locks and semaphores
lock_t buffer_lock;
sem_t empty_slots;
sem_t full_slots;


// circular buffer to hold the produced items
#define BUFFER_SIZE 10
char buffer[BUFFER_SIZE];
int start = 0; // index for next item to produce
int end = 0; // index for next item to consume
int count = 0; // number of items in the buffer


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

    buffer_lock = LockCreate();
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
    // use the locks and semaphores to add the item to the buffer
    LockAcquire(buffer_lock);
    
}
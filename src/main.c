#include <ctype.h>
#include <curses.h>
#include <form.h>
#include <ncurses.h>
#include <panel.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLUMNS 3
#define COLUMNS_GAP 2
#define CARD_HEIGHT 8
#define CARDS_GAP 2

typedef enum { PRIORITY_LOW, PRIORITY_NORMAL, PRIORITY_HIGH } Priority;

typedef struct Card Card;
typedef struct CardSlot CardSlot;
typedef struct Column Column;
typedef struct KanbanBoard KanbanBoard;

struct Card {
	char *title;
	char *description;
	Priority priority;
	Card *prev;
	Card *next;
};

struct CardSlot {
	WINDOW *window;
	PANEL *panel;
	Card *data;
	bool visible;
};

struct Column {
	WINDOW *window;
	PANEL *panel;
	char *title;
	CardSlot *cardSlots;
	size_t nCardSlots;
	size_t cardOffset;
	size_t selectedIdx;
	Card *cards;
	Card *selectedCard;
	size_t nCards;
};

struct KanbanBoard {
	Column columns[3];
	unsigned char selectedColumn;
};

typedef struct {
	char title[256];
	char description[256];
	Priority priority;
} newCardFormData;

void debugLog(const char *format, ...) {
	FILE *log_file = fopen("debug.log", "a");
	if (log_file) {
		va_list args;
		va_start(args, format);
		vfprintf(log_file, format, args);
		va_end(args);
		fclose(log_file);
	}
}

char *trimWhitespace(char *str) {
	char *end;
	while (isspace((unsigned char)*str))
		str++;
	if (*str == 0)
		return str;
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;
	end[1] = '\0';
	return str;
}

void initNcurses() {
	initscr();
	start_color();
	cbreak();
	keypad(stdscr, TRUE);
	noecho();
	curs_set(FALSE);
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(3, COLOR_GREEN, COLOR_BLACK);
	init_pair(4, COLOR_YELLOW, COLOR_BLACK);
	init_pair(5, COLOR_RED, COLOR_BLACK);
	refresh();
}

void setColumnFocus(KanbanBoard *board, unsigned char target) {
	if (board == NULL || target > 2)
		return;
	for (size_t i = 0; i < 3; i++) {
		wattron(board->columns[i].window, COLOR_PAIR(((i == target) ? 2 : 1)));
		box(board->columns[i].window, 0, 0);
		if (board->columns[i].panel) {
			mvwprintw(
				board->columns[i].window, 0, 2, "%s", board->columns[i].title);
		}
		wattroff(board->columns[i].window, COLOR_PAIR(((i == target) ? 2 : 1)));
	}
	board->selectedColumn = target;
}

void updateVisibleCards(KanbanBoard *board, unsigned char targetColumn) {
	if (board == NULL || targetColumn > 2) {
		return;
	}
	Column *currentColumn = &board->columns[targetColumn];
	for (size_t i = 0; i < currentColumn->nCardSlots; i++) {
		werase(currentColumn->cardSlots[i].window);
		currentColumn->cardSlots[i].data = NULL;
	}
	if (currentColumn->selectedIdx
		>= currentColumn->cardOffset + currentColumn->nCardSlots) {
		currentColumn->cardOffset++;
	} else if (currentColumn->selectedIdx < currentColumn->cardOffset) {
		currentColumn->cardOffset--;
	}
	Card *card = currentColumn->cards;
	size_t count = 0;
	while (card != NULL) {
		if (count == currentColumn->cardOffset) {
			break;
		}
		count++;
		card = card->next;
	}
	size_t cardSlotCount = 0;
	while (card != NULL) {
		if (cardSlotCount >= currentColumn->nCardSlots)
			break;
		CardSlot *slot = &currentColumn->cardSlots[cardSlotCount];
		slot->data = card;
		werase(slot->window);
		wattron(
			slot->window,
			COLOR_PAIR(((card->priority == PRIORITY_LOW)
							? 3
							: ((card->priority == PRIORITY_NORMAL) ? 4 : 5))));
		if (count == currentColumn->selectedIdx) {
			wattron(slot->window, A_REVERSE);
		}
		box(slot->window, 0, 0);
		mvwprintw(slot->window, 0, 2, "%.*s", 6, card->title);
		wattroff(
			slot->window,
			COLOR_PAIR(((card->priority == PRIORITY_LOW)
							? 3
							: ((card->priority == PRIORITY_NORMAL) ? 4 : 5))));
		if (count == currentColumn->selectedIdx) {
			wattroff(slot->window, A_REVERSE);
		}
		card = card->next;
		cardSlotCount++;
		count++;
	}
	doupdate();
	update_panels();
}

void createKanbanBoard(KanbanBoard *board) {
	if (board == NULL)
		return;
	unsigned int columnsHeight = LINES - (COLUMNS_GAP * 2);
	unsigned int columnsWidth = (COLS / COLUMNS) - (COLUMNS_GAP * 2);
	char *titles[] = {"To-Do", "In Progress", "Done"};
	for (size_t i = 0; i < 3; i++) {
		Column *currentColumn = &board->columns[i];
		currentColumn->window =
			newwin(columnsHeight,
				   columnsWidth,
				   2,
				   2 + ((i > 0) ? ((i * columnsWidth) + (2 * i)) : 0));
		wattron(currentColumn->window, COLOR_PAIR(1));
		box(currentColumn->window, 0, 0);
		wattroff(currentColumn->window, COLOR_PAIR(1));
		currentColumn->title = calloc(strlen(titles[i]) + 1, sizeof(char));
		strcpy(currentColumn->title, titles[i]);
		size_t visibleCards = ((float)columnsHeight / (2 + CARD_HEIGHT));
		currentColumn->cardSlots = calloc(visibleCards, sizeof(CardSlot));
		currentColumn->nCardSlots = visibleCards;
		for (size_t s = 0; s < visibleCards; s++) {
			CardSlot *currentCardSlot = &currentColumn->cardSlots[s];
			int cardY = 2 + (s * (CARD_HEIGHT + 1));
			int cardX = 2;
			currentCardSlot->window = derwin(currentColumn->window,
											 CARD_HEIGHT,
											 columnsWidth - 4,
											 cardY,
											 cardX);
			currentCardSlot->panel = new_panel(currentCardSlot->window);
			currentCardSlot->data = NULL;
			currentCardSlot->visible = false;
			top_panel(currentCardSlot->panel);
		}
		currentColumn->cardOffset = 0;
		currentColumn->selectedIdx = 0;
		currentColumn->cards = NULL;
		currentColumn->selectedCard = NULL;
		currentColumn->nCards = 0;
		currentColumn->panel = new_panel(currentColumn->window);
	}
	board->selectedColumn = 0;
	setColumnFocus(board, 0);
	update_panels();
	doupdate();
}

void freeKanbanBoard(KanbanBoard *board) {
	if (board == NULL)
		return;
	for (size_t i = 0; i < 3; i++) {
		if (board->columns[i].title) {
			free(board->columns[i].title);
			board->columns[i].title = NULL;
		}
		if (board->columns[i].cardSlots) {
			for (size_t s = 0; s < board->columns[i].nCardSlots; s++) {
				if (board->columns[i].cardSlots[s].panel) {
					del_panel(board->columns[i].cardSlots[s].panel);
				}
				if (board->columns[i].cardSlots[s].window) {
					delwin(board->columns[i].cardSlots[s].window);
				}
			}
			free(board->columns[i].cardSlots);
			board->columns[i].cardSlots = NULL;
			board->columns[i].nCardSlots = 0;
		}
		Card *currentCard = board->columns[i].cards;
		while (currentCard != NULL) {
			Card *next = currentCard->next;
			if (currentCard->title) {
				free(currentCard->title);
				currentCard->title = NULL;
			}
			if (currentCard->description) {
				free(currentCard->description);
				currentCard->description = NULL;
			}
			free(currentCard);
			currentCard = next;
		}
		board->columns[i].cards = NULL;
		board->columns[i].nCards = 0;
		if (board->columns[i].panel) {
			del_panel(board->columns[i].panel);
			board->columns[i].panel = NULL;
		}
		if (board->columns[i].window) {
			delwin(board->columns[i].window);
			board->columns[i].window = NULL;
		}
	}
}

void createCard(KanbanBoard *board,
				unsigned char targetColumn,
				char *title,
				char *description,
				Priority priority) {
	if (!board || targetColumn > 2) {
		return;
	}
	Column *currentColumn = &board->columns[targetColumn];
	Card *card = calloc(1, sizeof(Card));
	if (!card) {
		return;
	}
	card->title = calloc(strlen(title) + 1, sizeof(char));
	card->description = calloc(strlen(description) + 1, sizeof(char));
	strcpy(card->title, title);
	strcpy(card->description, description);
	card->priority = priority;
	if (currentColumn->cards == NULL) {
		currentColumn->cards = card;
	} else {
		Card *currentCard = currentColumn->cards;
		while (currentCard->next != NULL) {
			currentCard = currentCard->next;
		}
		currentCard->next = card;
		card->prev = currentCard;
	}
	currentColumn->selectedCard = card;
	currentColumn->selectedIdx = currentColumn->nCards;
	currentColumn->nCards++;
	updateVisibleCards(board, targetColumn);
}

void destroyCard(KanbanBoard *board,
				 unsigned char targetColumn,
				 Card *targetCard) {
	if (board == NULL || targetCard == NULL || targetColumn > 2)
		return;
	Column *currentColumn = &board->columns[targetColumn];
	if (targetCard->prev == NULL) {
		currentColumn->cards = targetCard->next;
		if (currentColumn->cards != NULL) {
			currentColumn->cards->prev = NULL;
		}
	} else {
		targetCard->prev->next = targetCard->next;
	}
	if (targetCard->next != NULL) {
		targetCard->next->prev = targetCard->prev;
	}
	if (currentColumn->selectedCard == targetCard) {
		if (targetCard->prev != NULL) {
			currentColumn->selectedCard = targetCard->prev;
			currentColumn->selectedIdx--;
		} else if (targetCard->next != NULL) {
			currentColumn->selectedCard = targetCard->next;
			currentColumn->selectedIdx++;
		} else {
			currentColumn->selectedCard = NULL;
			currentColumn->selectedIdx = 0;
		}
	}
	currentColumn->nCards--;
	free(targetCard->title);
	free(targetCard->description);
	free(targetCard);
	updateVisibleCards(board, targetColumn);
	update_panels();
	doupdate();
}

void moveCardBetweenColumns(KanbanBoard *board,
							unsigned char currentCardColumn,
							Card *targetCard,
							unsigned char targetColumn) {
	if (board == NULL || currentCardColumn > 2 || targetCard == NULL
		|| targetColumn > 2)
		return;
	char *title = calloc(strlen(targetCard->title) + 1, sizeof(char));
	char *description =
		calloc(strlen(targetCard->description) + 1, sizeof(char));
	strcpy(title, targetCard->title);
	strcpy(description, targetCard->description);
	Priority priority = targetCard->priority;
	destroyCard(board, currentCardColumn, targetCard);
	createCard(board, targetColumn, title, description, priority);
	free(title);
	free(description);
	board->columns[currentCardColumn].selectedIdx--;
}

bool showNewCardForm(newCardFormData *newCardData) {
	if (!newCardData)
		return false;
	FIELD *fields[4];
	FORM *form;
	WINDOW *window;
	WINDOW *subwin;
	PANEL *panel;
	int rows, cols;
	fields[0] = new_field(1, 40, 2, 15, 0, 0);
	if (!fields[0]) {
		fprintf(stderr, "Error to create field 0 to new card form\n");
		return false;
	}
	fields[1] = new_field(3, 40, 5, 15, 0, 0);
	if (!fields[1]) {
		fprintf(stderr, "Error to create field 1 to new card form\n");
		free_field(fields[0]);
		return false;
	}
	fields[2] = new_field(1, 15, 9, 20, 0, 0);
	if (!fields[2]) {
		fprintf(stderr, "Error to create field 2 to new card form\n");
		free_field(fields[0]);
		free_field(fields[1]);
		return false;
	}
	fields[3] = NULL;
	form = new_form(fields);
	if (!form) {
		fprintf(stderr, "Error to create form to new card form\n");
		free_field(fields[0]);
		free_field(fields[1]);
		free_field(fields[2]);
		return false;
	}
	scale_form(form, &rows, &cols);
	int formHeight = rows + 6;
	int formWidth = cols + 4;
	int startY = (LINES - formHeight) / 2;
	int startX = (COLS - formWidth) / 2;
	window = newwin(formHeight, formWidth, startY, startX);
	if (!window) {
		fprintf(stderr, "Error to create window to new card form\n");
		free_form(form);
		free_field(fields[0]);
		free_field(fields[1]);
		free_field(fields[2]);
		return false;
	}
	panel = new_panel(window);
	if (!panel) {
		fprintf(stderr, "Error to create panel to new card form\n");
		free_form(form);
		free_field(fields[0]);
		free_field(fields[1]);
		free_field(fields[2]);
		delwin(window);
		return false;
	}
	keypad(window, TRUE);
	subwin = derwin(window, formHeight - 2, formWidth - 2, 1, 1);
	set_form_win(form, window);
	set_form_sub(form, subwin);
	set_field_back(fields[0], A_UNDERLINE);
	set_field_back(fields[1], A_UNDERLINE);
	set_field_back(fields[2], A_UNDERLINE);
	for (int i = 0; i < 3; i++) {
		field_opts_on(fields[i], O_VISIBLE);
		field_opts_on(fields[i], O_ACTIVE);
		field_opts_on(fields[i], O_PUBLIC);
		field_opts_on(fields[i], O_EDIT);
	}
	set_field_type(fields[0], TYPE_ALNUM, 0);
	set_field_type(fields[1], TYPE_ALNUM, 0);
	char *options[] = {"Low", "Normal", "High", NULL};
	set_field_type(fields[2], TYPE_ENUM, options, 0, 1);
	set_field_buffer(fields[0], 0, "");
	set_field_buffer(fields[1], 0, "");
	set_field_buffer(fields[2], 0, "Normal");
	box(window, 0, 0);
	mvwprintw(window, 0, 2, "New Card");
	post_form(form);
	refresh();
	top_panel(panel);
	update_panels();
	doupdate();
	mvwprintw(subwin, 2, 2, "Title:");
	mvwprintw(subwin, 5, 2, "Description:");
	mvwprintw(subwin, 9, 2, "Priority (L/N/H):");
	mvwprintw(window, formHeight - 2, 2, "F1-Save  F2-Cancel");
	form_driver(form, REQ_FIRST_FIELD);
	bool formDone = false;
	bool result = false;
	int ch;
	while (!formDone) {
		ch = wgetch(window);
		switch (ch) {
			case KEY_DOWN:
				form_driver(form, REQ_NEXT_FIELD);
				break;
			case KEY_UP:
				form_driver(form, REQ_PREV_FIELD);
				break;
			case KEY_LEFT:
				form_driver(form, REQ_PREV_CHAR);
				break;
			case KEY_RIGHT:
				form_driver(form, REQ_NEXT_CHAR);
				break;
			case KEY_BACKSPACE:
			case 127:
				form_driver(form, REQ_DEL_PREV);
				break;
			case KEY_DC:
				form_driver(form, REQ_DEL_CHAR);
				break;
			case KEY_HOME:
				form_driver(form, REQ_BEG_FIELD);
				break;
			case KEY_END:
				form_driver(form, REQ_END_FIELD);
				break;
			case 9:
				form_driver(form, REQ_NEXT_FIELD);
				break;
			case KEY_BTAB:
				form_driver(form, REQ_PREV_FIELD);
				break;
			case ' ':
				form_driver(form, REQ_INS_CHAR);
				break;
			case KEY_F(1):
				form_driver(form, REQ_VALIDATION);
				char title_buffer[41];
				char desc_buffer[121];
				char prio_buffer[16];
				strncpy(title_buffer, field_buffer(fields[0], 0), 40);
				title_buffer[40] = '\0';
				char *title = trimWhitespace(title_buffer);
				strncpy(desc_buffer, field_buffer(fields[1], 0), 120);
				desc_buffer[120] = '\0';
				char *description = trimWhitespace(desc_buffer);
				strncpy(prio_buffer, field_buffer(fields[2], 0), 15);
				prio_buffer[15] = '\0';
				char *priority = trimWhitespace(prio_buffer);
				strncpy(
					newCardData->title, title, sizeof(newCardData->title) - 1);
				newCardData->title[sizeof(newCardData->title) - 1] = '\0';
				strncpy(newCardData->description,
						description,
						sizeof(newCardData->description) - 1);
				newCardData->description[sizeof(newCardData->description) - 1] =
					'\0';
				if (strcmp(priority, "Low") == 0)
					newCardData->priority = PRIORITY_LOW;
				else if (strcmp(priority, "High") == 0)
					newCardData->priority = PRIORITY_HIGH;
				else
					newCardData->priority = PRIORITY_NORMAL;
				formDone = true;
				result = true;
				break;
			case KEY_F(2):
				formDone = true;
				result = false;
				break;
			case 27:
				formDone = true;
				result = false;
				break;
			default:
				if (ch >= 32 && ch <= 126) {
					form_driver(form, ch);
				} else {
					beep();
				}
				break;
		}
		update_panels();
		doupdate();
	}
	unpost_form(form);
	free_form(form);
	for (int i = 0; i < 3; i++) {
		free_field(fields[i]);
	}
	del_panel(panel);
	delwin(subwin);
	delwin(window);
	return result;
}

int main(void) {
	initNcurses();
	KanbanBoard kanban;
	createKanbanBoard(&kanban);

	while (true) {
		int c = getch();
		if (c == 'q' || c == 'Q') {
			break;
		} else if (c == KEY_LEFT) {
			if (kanban.selectedColumn > 0) {
				setColumnFocus(&kanban, kanban.selectedColumn - 1);
			}
		} else if (c == KEY_RIGHT) {
			if (kanban.selectedColumn < 2) {
				setColumnFocus(&kanban, kanban.selectedColumn + 1);
			}
		} else if (c == KEY_UP) {
			if (kanban.columns[kanban.selectedColumn].selectedIdx > 0) {
				kanban.columns[kanban.selectedColumn].selectedIdx--;
				updateVisibleCards(&kanban, kanban.selectedColumn);
			}
		} else if (c == KEY_DOWN) {
			if (kanban.columns[kanban.selectedColumn].selectedIdx
				< kanban.columns[kanban.selectedColumn].nCards - 1) {
				kanban.columns[kanban.selectedColumn].selectedIdx++;
				updateVisibleCards(&kanban, kanban.selectedColumn);
			}
		} else if (c == 'n' || c == 'N') {
			newCardFormData data;
			if (showNewCardForm(&data)) {
				createCard(&kanban,
						   kanban.selectedColumn,
						   data.title,
						   data.description,
						   data.priority);
			}
		} else if (c == 'd' || c == 'D') {
			if (kanban.columns[kanban.selectedColumn].selectedCard != NULL) {
				destroyCard(&kanban,
							kanban.selectedColumn,
							kanban.columns[kanban.selectedColumn].selectedCard);
			}
		} else if (c == 'm' || c == 'M') {
			int c2 = getch();
			if (c2 == KEY_RIGHT && kanban.selectedColumn < 2) {
				moveCardBetweenColumns(
					&kanban,
					kanban.selectedColumn,
					kanban.columns[kanban.selectedColumn].selectedCard,
					kanban.selectedColumn + 1);
			} else if (c2 == KEY_LEFT && kanban.selectedColumn > 0) {
				moveCardBetweenColumns(
					&kanban,
					kanban.selectedColumn,
					kanban.columns[kanban.selectedColumn].selectedCard,
					kanban.selectedColumn - 1);
			}
		}

		update_panels();
		doupdate();
	}

	freeKanbanBoard(&kanban);
	endwin();

	return EXIT_SUCCESS;
}

#include <stdlib.h>

#include "global.h"
#include "debug.h"

/**
 * @brief  Read genetic distance data and initialize data structures.
 * @details  This function reads genetic distance data from a specified
 * input stream, parses and validates it, and initializes internal data
 * structures.
 *
 * The input format is a simplified version of Comma Separated Values
 * (CSV).  Each line consists of text characters, terminated by a newline.
 * Lines that start with '#' are considered comments and are ignored.
 * Each non-comment line consists of a nonempty sequence of data fields;
 * each field iis terminated either by ',' or else newline for the last
 * field on a line.  The constant INPUT_MAX specifies the maximum number
 * of data characters that may be in an input field; fields with more than
 * that many characters are regarded as invalid input and cause an error
 * return.  The first field of the first data line is empty;
 * the subsequent fields on that line specify names of "taxa", which comprise
 * the leaf nodes of a phylogenetic tree.  The total number N of taxa is
 * equal to the number of fields on the first data line, minus one (for the
 * blank first field).  Following the first data line are N additional lines.
 * Each of these lines has N+1 fields.  The first field is a taxon name,
 * which must match the name in the corresponding column of the first line.
 * The subsequent fields are numeric fields that specify N "distances"
 * between this taxon and the others.  Any additional lines of input following
 * the last data line are ignored.  The distance data must form a symmetric
 * matrix (i.e. D[i][j] == D[j][i]) with zeroes on the main diagonal
 * (i.e. D[i][i] == 0).
 *
 * If 0 is returned, indicating data successfully read, then upon return
 * the following global variables and data structures have been set:
 *   num_taxa - set to the number N of taxa, determined from the first data line
 *   num_all_nodes - initialized to be equal to num_taxa
 *   num_active_nodes - initialized to be equal to num_taxa
 *   node_names - the first N entries contain the N taxa names, as C strings
 *   distances - initialized to an NxN matrix of distance values, where each
 *     row of the matrix contains the distance data from one of the data lines
 *   nodes - the "name" fields of the first N entries have been initialized
 *     with pointers to the corresponding taxa names stored in the node_names
 *     array.
 *   active_node_map - initialized to the identity mapping on [0..N);
 *     that is, active_node_map[i] == i for 0 <= i < N.
 *
 * @param in  The input stream from which to read the data.
 * @return 0 in case the data was successfully read, otherwise -1
 * if there was any error.  Premature termination of the input data,
 * failure of each line to have the same number of fields, and distance
 * fields that are not in numeric format should cause a one-line error
 * message to be printed to stderr and -1 to be returned.
 */
void refresh_input_buffer() {
    int i =0;
    while (*(input_buffer+i) != '\0') {
        *(input_buffer+i) = '\0';
        i++;
    }
}

double char_to_double(char* original) {
    double res = 0;
    double dot = 1;
    int point = 0;
    int i = 0;
    while (*(original+i) != '\0') {
        if (*(original+i) == '.'){
          point = 1;
          i++;
          continue;
        }
        int d = *(original+i) - '0';
        if (d >= 0 && d <= 9){
            if (point) dot /= 10.0f;
            res = res * 10.0f + (double)d;
        }
        i++;
    }
    refresh_input_buffer();
    // printf("\nchar to double func");
    return res * dot;
}

void put_active_node(int index, int indicies) {
    active_node_map[index] = indicies;
}

int read_distance_data(FILE *in) {
    char check = fgetc(in);
    num_taxa = 0;
    int ctr = 0;
    int read_first_line = 0;
    int a = 0;
    int b = 0;
    //printf("\n test: %d", check);

    while (check != EOF) {
        // printf("\ninput: %c", check);
        if (check == ',' && read_first_line == 0) {
            while(check != '\n') {
                int i = 0;
                check = fgetc(in);
                while (check != ',' && check != '\n') {
                    if (i > 100) {
                        return -1;
                    }
                    *(*(node_names + ctr)+i) = check;
                    // printf("\ninput into array: %c", *(*(node_names + ctr)+i));
                    // printf("\nnode: %d", ctr);
                    check = fgetc(in);
                    i++;
                }
                // printf("\ninput should be , or newline %c", check);
                *(*(node_names + ctr)+i) = '\0';
                (*(nodes+ctr)).name = *(node_names + ctr); // make the read node name as a node
                // printf("\n%s",(*(nodes+ctr)).name);
                active_node_map[ctr] = ctr; // put into active node map
                ctr++; //checks how many leaf nodes are put in
                num_taxa++;
            }
            read_first_line = 1;
        }
        else if (check == '#') {
            check = fgetc(in);
            while (check != '\n') {
                check = fgetc(in);
            }
        }
        else if (check == ',') {
            // printf("\ncomma ");
            a++;
            check = fgetc(in);
        }
        else if (check == '\n') {
            // printf("\n newline ");
            check = fgetc(in);
            // b++;
            a=0;
            continue;
        }
        else if (a != 0) {
            if (a == b) { //checks if the diagonal line is 0
                if (check != '0') {
                    // printf("\na: %d", a);
                    // printf("\nb: %d", b);
                    // printf("\n not 0 at diagonal");
                    return -1;
                }
            }
            if (check == '-') {
                return -1;
            }
            int inc = 0;
            while (check != ',' && check != '\n' && check != EOF) {
                *(input_buffer+inc) = check;
                inc++;
                check = fgetc(in);
            }
            *(input_buffer+inc) = '\0';
            double distance = char_to_double(input_buffer);
            // printf("\n distance [%d][%d]= %f",b-1,a-1, distance);
            *(*(distances + b-1) + a-1) = distance;
        }
        else {
            // printf("\nstart comparing input names");
            b++;
            a = 0;
            int actr = 0;
            while (check != ',') {
                *(input_buffer+actr) = check;
                actr++;
                check = fgetc(in);
            }
            *(input_buffer+actr) = '\0';
            char c = *(input_buffer);
            actr=0;
            while (c != '\0') {
                c = *(input_buffer+actr);
                if(c != *(*(node_names + b-1)+actr)) {
                    // printf("\n fail comparason1");
                    return -1;
                }
                actr++;
            }
            actr=0;
            while (*(*(node_names + b-1)+actr) != '\0') {
                c = *(input_buffer+actr);
                if(c != *(*(node_names + b-1)+actr)) {
                    // printf("\n fail comparason2");
                    return -1;
                }
                actr++;
            }
            refresh_input_buffer();
        }
    }
    // printf("\ndone with reading input");
    a=0;
    b=0;
    num_all_nodes = num_taxa;
    num_active_nodes = num_taxa;
    if (num_taxa > MAX_TAXA) return -1;
    while (num_taxa != b) {
        if (*(*(distances+b)+a) != *(*(distances+a)+b)) {
            return -1;
        }
        else {
            a++;
            if (a == num_taxa) {
                b++;
                a = 0;
            }
        }
    }
    return 0;
}
/**
 * @brief  Emit a representation of the phylogenetic tree in Newick
 * format to a specified output stream.
 * @details  This function emits a representation in Newick format
 * of a synthesized phylogenetic tree to a specified output stream.
 * See (https://en.wikipedia.org/wiki/Newick_format) for a description
 * of Newick format.  The tree that is output will include for each
 * node the name of that node and the edge distance from that node
 * its parent.  Note that Newick format basically is only applicable
 * to rooted trees, whereas the trees constructed by the neighbor
 * joining method are unrooted.  In order to turn an unrooted tree
 * into a rooted one, a root will be identified according by the
 * following method: one of the original leaf nodes will be designated
 * as the "outlier" and the unique node adjacent to the outlier
 * will serve as the root of the tree.  Then for any other two nodes
 * adjacent in the tree, the node closer to the root will be regarded
 * as the "parent" and the node farther from the root as a "child".
 * The outlier node itself will not be included as part of the rooted
 * tree that is output.
 *
 * @param out  Stream to which to output a rooted tree represented in
 * Newick format.
 * @param x  Pointer to the leaf node to be regarded as the "outlier".
 * The unique node adjacent to the outlier will be the root of the tree
 * that is output.  The outlier node itself will not be part of the tree
 * that is emitted.
 */
int max_distance() {
    double distance = 0.0;
    int max_i  = 0;
    int max_j = 0;
    for(int i=0; i<num_taxa;i++) {
        for(int j =i; j<num_taxa;j++) {
            if(i == j) {
                continue;
            }
            if(*(*(distances+i)+j) > distance) {
                max_i = i;
                max_j = j;
                distance = *(*(distances+i)+j);
            }
        }
    }
    if (max_i<max_j) {
        return max_i;
    }
    return max_j;
}

int get_outlier_index() {
    int outlier_index = max_distance();
    if (*outlier_name == '\0') { // setting
        for(int i=0; i<num_taxa;i++) {
            if (outlier_index == i) {
                outlier_name = *(node_names+i);
            }
        }
    }
    else { // setting index and returning -1 if outlier doesn't exist
        outlier_index = -1;
        for (int i=0; i<num_taxa; i++) {
            int ptr=0;
            while(*(*(node_names+i)+ptr) != '\0') {
                if (*(outlier_name+ptr) == *(*(node_names+i)+ptr)) {
                    outlier_index = i;
                    // printf("\n node_name compare: %c",*(*(node_names+i)+ptr));
                }
                else {
                    outlier_index = -1;
                }
                ptr++;
            }
            if (outlier_index != -1) {
                outlier_index = -1;
                ptr = 0;
                while(*(outlier_name+ptr) != '\0') {
                    if (*(outlier_name+ptr) == *(*(node_names+i)+ptr)) {
                        outlier_index = i;
                    }
                    else {
                        outlier_index = -1;
                    }
                    ptr++;
                }
            }
            if (outlier_index != -1) {
                // printf("\nsuccess");
                break;
            }
        }
        if (outlier_index == -1) {
            // printf("\nfail");
            return -1;
        }
    }
    // printf("\n%s\n", outlier_name);
    return outlier_index;
}

int get_node_index(NODE* node) {
    int index = -1;
    for (int i=0; i<num_all_nodes; i++) {
        int ptr=0;
        // printf("start loop\n");
        while(*(*(node_names+i)+ptr) != '\0') {
            if (*(node->name+ptr) == *(*(node_names+i)+ptr)) {
                index = i;
                // printf("node_name compare: %c\n",*(*(node_names+i)+ptr));
            }
            else {
                index = -1;
            }
            ptr++;
        }
        if (index != -1) {
            index = -1;
            ptr = 0;
            while(*(node->name+ptr) != '\0') {
                if (*(node->name+ptr) == *(*(node_names+i)+ptr)) {
                    index = i;
                }
                else {
                    index = -1;
                }
                ptr++;
            }
        }
        if (index != -1) {
            // printf("correct\n");
            break;
        }
    }
    if (index == -1) {
        // printf("!!!something wrong\n");
        return index;
    }
    return index;
}

void recursive_get_leaf(FILE* out, NODE* node1, NODE* node2) {
    for (int i=0; i<3; i++) {
        if (*(node2->neighbors+i) == NULL || *(node2->neighbors+i) == node1) { //if null or parent skip
            continue;
        }
        else {
            if (i == 0 && *(node2->neighbors+i) != node1) { //if not parent && beginning
                fprintf(out,"(");
            }
            else if (i == 1 && *(node2->neighbors+i-1) == node1) {
                fprintf(out,"(");
            }
            else {
                fprintf(out, ",");
            }
            recursive_get_leaf(out, node2, *(node2->neighbors+i));
            if (i == 2 || (i == 1 && *(node2->neighbors+i+1) == node1)) {
                fprintf(out, ")");
            }
        }
    }
    int j = get_node_index(node2);
    int k = get_node_index(node1);
    fprintf(out, "%s:%.2f", node2->name, *(*(distances+j)+k));
}

int emit_newick_format(FILE *out) {
    int success = build_taxonomy(out);
    if (success != 0) {
        return -1;
    }
    int outlier_index = get_outlier_index();
    if (outlier_index == -1) {
        return -1;
    }

    NODE* u = nodes+outlier_index; //set original to u
    recursive_get_leaf(out, u, *u->neighbors);
    fprintf(out,"\n");

    return 0;
}

/**
 * @brief  Emit the synthesized distance matrix as CSV.
 * @details  This function emits to a specified output stream a representation
 * of the synthesized distance matrix resulting from the neighbor joining
 * algorithm.  The output is in the same CSV form as the program input.
 * The number of rows and columns of the matrix is equal to the value
 * of num_all_nodes at the end of execution of the algorithm.
 * The submatrix that consists of the first num_leaves rows and columns
 * is identical to the matrix given as input.  The remaining rows and columns
 * contain estimated distances to internal nodes that were synthesized during
 * the execution of the algorithm.
 *
 * @param out  Stream to which to output a CSV representation of the
 * synthesized distance matrix.
 */
int emit_distance_matrix(FILE *out) {
    int success = build_taxonomy(out);
    if (success != 0) {
        return -1;
    }
    int row = 0;
    int col = 0;
    int n = 0;
    while(n < num_all_nodes) {
        fprintf(out, ",%s", *(node_names+n));
        n++;
    }
    n++;
    fprintf(out, "%s\n", *(node_names+n)); //finish printing first row
    while(row<num_all_nodes) {
        if (col == 0) {
            fprintf(out,"%s", *(node_names+row));
        }
        fprintf(out, ",%.2f", *(*(distances+row)+col));
        if (col == num_all_nodes-1) {
            row++;
            col = 0;
            fprintf(out,"\n");
        }
        else {
            col++;
        }
    }
    return 0;
}

/**
 * @brief  Build a phylogenetic tree using the distance data read by
 * a prior successful invocation of read_distance_data().
 * @details  This function assumes that global variables and data
 * structures have been initialized by a prior successful call to
 * read_distance_data(), in accordance with the specification for
 * that function.  The "neighbor joining" method is used to reconstruct
 * phylogenetic tree from the distance data.  The resulting tree is
 * an unrooted binary tree having the N taxa from the original input
 * as its leaf nodes, and if (N > 2) having in addition N-2 synthesized
 * internal nodes, each of which is adjacent to exactly three other
 * nodes (leaf or internal) in the tree.  As each internal node is
 * synthesized, information about the edges connecting it to other
 * nodes is output.  Each line of output describes one edge and
 * consists of three comma-separated fields.  The first two fields
 * give the names of the nodes that are connected by the edge.
 * The third field gives the distance that has been estimated for
 * this edge by the neighbor-joining method.  After N-2 internal
 * nodes have been synthesized and 2*(N-2) corresponding edges have
 * been output, one final edge is output that connects the two
 * internal nodes that still have only two neighbors at the end of
 * the algorithm.  In the degenerate case of N=1 leaf, the tree
 * consists of a single leaf node and no edges are output.  In the
 * case of N=2 leaves, then no internal nodes are synthesized and
 * just one edge is output that connects the two leaves.
 *
 * Besides emitting edge data (unless it has been suppressed),
 * as the tree is built a representation of it is constructed using
 * the NODE structures in the nodes array.  By the time this function
 * returns, the "neighbors" array for each node will have been
 * initialized with pointers to the NODE structure(s) for each of
 * its adjacent nodes.  Entries with indices less than N correspond
 * to leaf nodes and for these only the neighbors[0] entry will be
 * non-NULL.  Entries with indices greater than or equal to N
 * correspond to internal nodes and each of these will have non-NULL
 * pointers in all three entries of its neighbors array.
 * In addition, the "name" field each NODE structure will contain a
 * pointer to the name of that node (which is stored in the corresponding
 * entry of the node_names array).
 *
 * @param out  If non-NULL, an output stream to which to emit the edge data.
 * If NULL, then no edge data is output.
 */
double get_row_sum (int index) {
    // printf("\n inside get_row_sum");
    double sum = 0.0;
    for(int i=0; i<num_active_nodes; i++) {
        if (*(active_node_map+i) != -1) {
            sum += *(*(distances + index)+*(active_node_map+i));
        }
    }
    // printf("\nsum of row: %f", sum);
    return sum;
}

void int_to_char(int new_node_index) {
    while (new_node_index != 0) {
        char c = (new_node_index % 10) + '0';
        // printf("\n %c", c);
        if (new_node_index >= 100) {
            *(input_buffer+3) = c;
        }
        if (new_node_index >= 10) {
            *(input_buffer+2) = c;
        }
        else {
            *(input_buffer+1) = c;
        }
        new_node_index /= 10;
    }
}

void make_distance_matrix(int q_lower, int q_upper) {//make distance matrix
    int row = 0;
    int col = num_all_nodes;
    while (row != num_all_nodes+1) { //add distance going down
        double distance = 0.0;
        int count = 0;
        int in_active = 0;
        while (count < num_active_nodes) { // check if the rows we're touching is part of the active node map
            if (*(active_node_map+count) == row) {
                in_active = 1;
            }
            count++;
        }
        if (in_active == 0) {
            distance = 0;
        }
        else {
            if (row == num_all_nodes) {
                distance = 0;
            }
            else if (row == q_lower) {
                distance = (*(*(distances+q_lower)+q_upper) + (*(row_sums+q_lower) - *(row_sums+q_upper))
                    / (num_active_nodes -2)) / 2;
            }
            else if (row == q_upper) {
                distance = (*(*(distances+q_lower)+q_upper) + (*(row_sums+q_upper) - *(row_sums+q_lower))
                    / (num_active_nodes -2)) / 2;
            }
            else {
                distance = (*(*(distances+q_lower)+row) + *(*(distances+q_upper)+row)
                    - *(*(distances+q_lower)+q_upper)) / 2;
            }
        }
        // printf("\ncheck distance: %f", distance);
        *(*(distances+col)+row) = distance;
        *(*(distances+row)+col) = distance;
        row++;
    }
}

int build_taxonomy(FILE *out) {
    if (num_all_nodes == 1) {
        if (global_options == 0) {
            fprintf(out, "%d,%d,%.2f\n", num_all_nodes-1, num_all_nodes-1, *(*(distances)));
        }
    }
    else if (num_all_nodes == 2) {
        *((*(nodes)).neighbors) = *((*(nodes+1)).neighbors);
        *((*(nodes+1)).neighbors) = *((*(nodes)).neighbors);

        if (global_options == 0) {
            fprintf(out, "%d,%d,%.2f\n", 0, 1, *(*(distances)+1));
        }
    }
    else {
        while (num_active_nodes > 2) {
            for (int index =0; index<num_active_nodes; index++) { // calculate rowsums and put in val
                if (*(active_node_map+index) != -1) {
                    *(row_sums+*(active_node_map+index)) = get_row_sum(*(active_node_map+index));
                    // printf("\nrow sum %d : %f", *(active_node_map+index),*(row_sums+*(active_node_map+index)) );
                }
            }
            int row = 0;
            int col = 0;
            double minQ = 1;
            int q_upper = 0;
            int q_lower = 0;
            int upper_index = 0;
            int lower_index = 0;
            int i = 0;
            int j = 0;
            while (i < num_active_nodes){ // calculating Q and finding min Q
                if (*(active_node_map+i) != -1) {
                    col = *(active_node_map+i);
                    j = i;
                    while (j < num_active_nodes) {
                        row = *(active_node_map+j);
                        if (col == row) {
                            j++;
                            continue;
                        }
                        double calculated = (num_active_nodes-2) * (*(*(distances+col)+row)) - *(row_sums+col) - *(row_sums+row);
                        // printf("\nrow: %d, col: %d, calculated: %f", row, col, calculated);
                        if (calculated < minQ){
                            minQ = calculated;
                            q_lower = col;
                            q_upper = row;
                            lower_index = i;
                            upper_index = j;
                        }
                        j++;
                    }
                }
                i++;
            }

            *input_buffer = '#';
            int_to_char(num_all_nodes);
            NODE* u = nodes + num_all_nodes;

            *((*(nodes+q_lower)).neighbors) = u;
            *((*(nodes+q_upper)).neighbors) = u;

            *(u->neighbors+1) = nodes+q_lower;
            *(u->neighbors+2) = nodes+q_upper;

            int ctr = 0;
            while ((*(input_buffer+ctr) != '\0')){
                *(*(node_names+num_all_nodes)+ctr) = *(input_buffer+ctr);
                ctr++;
            }
            u->name = *(node_names+num_all_nodes);

            make_distance_matrix(q_lower, q_upper);

            refresh_input_buffer();

            if (global_options == 0) {
                fprintf(out, "%d,%d,%.2f\n", q_lower, num_all_nodes, *(*(distances+q_lower)+num_all_nodes));
                fprintf(out, "%d,%d,%.2f\n", q_upper, num_all_nodes, *(*(distances+q_upper)+num_all_nodes));
            }

            *(active_node_map+lower_index)= num_all_nodes; // change active node map
            *(active_node_map+upper_index)= *(active_node_map+num_active_nodes-1);

            num_active_nodes--;//remember to get rid of two nodes and add one node
            num_all_nodes++; //make sure we put in the last added node

            *(active_node_map+num_active_nodes)= -1;
        }

        *((nodes+*(active_node_map+1))->neighbors) = (nodes+*(active_node_map));
        *((nodes+*(active_node_map))->neighbors) = (nodes+*(active_node_map+1));
        if (global_options == 0) {
            fprintf(out, "%d,%d,%.2f\n", *(active_node_map+1),*(active_node_map),*(*(distances+*(active_node_map+1))+*(active_node_map)));
        }
    }
    return 0;
}

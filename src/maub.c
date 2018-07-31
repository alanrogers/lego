/**
@file maub.c
@page maub
@author Alan R. Rogers and Daniel Tabin
@brief Bootstrap model averaging

# `maub`: model averaging using BEPE

Bootstrap model averaging was proposed by Buckland et al (Biometrics,
53(2):603-618). It can be used with weights provided by any method of
model selection. Model selection is applied to the real data and also
to a set of bootstrap replicates. The weight, \f$w_i\f$ of the i'th
model is the fraction of these data sets for which the i'th model
wins.

The model-averaged estimator of a parameter, \f$\theta\f$, is the
average across models, weighted by \f$w_i\f$, of the model-specific
estimates of \f$\theta\f$. Models in which \f$\theta\f$ does not
appear are omitted from the weighted average.

To construct confidence intervals, we average across models within
each bootstrap replicate to obtain a bootstrap distribution of
model-averaged estimates.

Usage: maub m1.bepe m2.bepe ... mK.bepe -F m1.flat m2.flat ... mK.flat

Here, the mX.bepe file refer to different models of population
history. Each of these files consists of a list of numbers, one on
each line. The first line refers to the real data, and each succeeding
line refers to a single bootstrap replicate. The numbers may be
generated by bepe or clic. They should be criteria for model
selection, defined so that low numbers indicate preferred models. I
will refer to these numbers as "badness" values.

After the `-F` argument comes a list of files, each of which can be
generated by @ref flatfile "flatfile.py". There must be f `.flat` file
for each model, so the number of `.flat` files should equal the number
of `.bepe` files. The first row of a `.flat` file is a header and
consists of column labels. Each column refers to a different
parameter, and the column labels are the names of these
parameters. The various `.flat` files need not agree about the number
of parameters or about the order of the parameters they share. But
shared parameters must have the same name in each `.flat` file.

After the header, each row in a `.flat` file refers to a different
data set. The first row after the header refers to the real data. Each
succeeding row refers to a bootstrap replicate. The number of rows
(excluding comments and the header) should agree with the numbers of
rows in the `.bepe` files.

In both types of input files, comments begin with a sharp character
and are ignored.

When `maub` runs, the first step is to calculate model weights,
\f$w_{i}\f$, where \f$i\f$ runs across models. The value of
\f$w_{ij}\f$ is the fraction data sets (i.e. of rows in the `.bepe`
files) for which \f$i\f$ is the best model (i.e. the one with the
lowest badness value.

In the next step, `maub` averages across models to obtain a
model-averaged estimate of each parameter. This is done separately for
each data set: first for the real data and then for each bootstrap
replicate. Some parameters may be missing from some models. In this
case, the average runs only across models that include the parameter,
and the weights are re-normalized so that they sum to 1 within this
reduced set of models.

Finally, the program uses the bootstrap distribution of model-averaged
parameter estimates to construct a 95% confidence interval for each
parameter. 

The program produces two output files. The first of these is written
to standard output and has the same form as the output of \ref
bootci "bootci.py". The first column consists of parameter names and the
2nd of model-averaged parameter estimates. The 3rd and 4th columns are
the lower and upper bounds of the confidence intervals.

The program also writes a file in the format of `ref flatfile
"flatfile.py". There is a header listing parameter labels. After the
header, row *i* gives the model-averaged estimate of each parameter
for the *i*th bootstrap replicate.

@copyright Copyright (c) 2018, Alan R. Rogers
<rogers@anthro.utah.edu>. This file is released under the Internet
Systems Consortium License, which can be found in file "LICENSE".
*/

typedef struct flat flat;
struct flat {
	int nparams;
	int nmodels;
    double** values;
    char** param_names;
};


#include "misc.h"
#include <string.h>

void usage(void);
double* maub_parse_bepe(const char* file_name);
int get_lines(const char* file_name);
void get_flats(const char** file_names, int nfiles, int nmodels, flat* flat_array);

const char *usageMsg =
    "Usage: maub m1.bepe m2.bepe ... mK.bepe -F m1.flat m2.flat ... mK.flat\n"
    "\n"
	"Here, the mX.bepe file refer to different models of population\n"
	"history. Each of these files consists of a list of numbers, one on\n"
	"each line. The first line refers to the real data, and each succeeding\n"
	"line refers to a single bootstrap replicate. The numbers may be\n"
	"generated by bepe or clic. They should be criteria for model\n"
	"selection, defined so that low numbers indicate preferred models. I\n"
	"will refer to these numbers as \"badness\" values.\n"
	"\n"
	"After the `-F` argument comes a list of files, each of which can be\n"
	"generated by @ref flatfile \"flatfile.py\". There must be f `.flat` file\n"
	"for each model, so the number of `.flat` files should equal the number\n"
	"of `.bepe` files. The first row of a `.flat` file is a header and\n"
	"consists of column labels. Each column refers to a different\n"
	"parameter, and the column labels are the names of these\n"
	"parameters. The various `.flat` files need not agree about the number\n"
	"of parameters or about the order of the parameters they share. But\n"
	"shared parameters must have the same name in each `.flat` file.\n"
	"\n"
	"After the header, each row in a `.flat` file refers to a different\n"
	"data set. The first row after the header refers to the real data. Each\n"
	"succeeding row refers to a bootstrap replicate. The number of rows\n"
	"(excluding comments and the header) should agree with the numbers of\n"
	"rows in the `.bepe` files.\n";

 void usage(void) {
     fputs(usageMsg, stderr);
     exit(EXIT_FAILURE);
 }

 int get_lines(const char* file_name){
 	FILE* f = fopen(file_name, "r");
    if(f==NULL) {
        fprintf(stderr,"%s:%d: can't read file \"%s\"\n",
                __FILE__,__LINE__,file_name);
        exit(EXIT_FAILURE);
    }

	char temp;
	int num_lines = 0;

	do {
	    temp = fgetc(f);
	    if(temp == '\n'){
	        num_lines++;
	    }
	} while (temp != EOF);

	fclose(f);
	return num_lines;
 }

 void get_flats(const char** file_names, int nfiles, int nmodels, flat* flat_array){
 	for (int i = 0; i < nfiles; i++){
 		FILE* f = fopen(file_names[i], "r");
	    if(f==NULL) {
	        fprintf(stderr,"%s:%d: can't read file \"%s\"\n",
	                __FILE__,__LINE__,file_names[i]);
	        exit(EXIT_FAILURE);
	    } 

	    char ch;
		int num_lines = 0;
		int params = 0;

		do {
		    ch = fgetc(f);
		    if(ch == '\n'){
		        num_lines++;
		    }
		} while (num_lines < 2);

		char* temp_params[1000];

		do {
			fscanf(f, "%s", temp_params[params]);
			params++;
		    ch = fgetc(f);
		} while (ch != '\n');

		flat_array[i].nparams = params;
		flat_array[i].nmodels = nmodels;
		flat_array[i].param_names = malloc(params*sizeof(char*));
		flat_array[i].values = malloc(nmodels*sizeof(double*));
		for (int j = 0; j < nmodels; j++){
			flat_array[i].values[j] =  malloc(params*sizeof(double));
		}
 	}
 }

 int main(int argc, char **argv){
  // Command line arguments specify file names
  if(argc < 4)
      usage();

  int nfiles = 0;
  int nmodels = 0;

  for(int i = 1; i < argc; i++){
    if (argv[i][0] == '-'){
      if(strcmp(argv[i], "-F") == 0){
        break;
      }
      else{
        usage();
      }
    }
    else{
      nfiles++;
    }
  }

  int nfiles_temp = 0;
  for(int i = (2+nfiles); i < argc; i++){
    if (argv[i][0] == '-'){
      usage();
    }
    else{
      nfiles_temp++;
    }
  }

  if(nfiles_temp != nfiles){
      fprintf(stderr, "%s:%d\n"
              " Inconsistent number of files!"
              " %d bepe files and %d flat files\n",
              __FILE__,__LINE__, nfiles, nfiles_temp);
      usage();
  }

  const char* bepe_file_names[nfiles];
  const char* flat_file_names[nfiles];

  FILE* bepe_files[nfiles];
  FILE* flat_files[nfiles];

  int* winner_totals[nmodels];

  for(int i = 0; i < nfiles; ++i)
      bepe_file_names[i] = argv[i+1];
  for(int i = 0; i < nfiles; ++i)
      flat_file_names[i] = argv[i+2+nfiles];

  for(int i = 0; i < nfiles; ++i)
      winner_totals[i] = 0;

  nmodels = get_lines(bepe_file_names[0]);
  for(int i = 0; i < nfiles; ++i) {
      if(get_lines(flat_file_names[i]) != nmodels) {
          fprintf(stderr, "%s:%d: inconsistent parameters in"
                  " files%s and %s\n", __FILE__,__LINE__,
                  flat_file_names[i], bepe_file_names[0]);
          exit(EXIT_FAILURE);
      }
      if(get_lines(bepe_file_names[i]) != nmodels) {
          fprintf(stderr, "%s:%d: inconsistent parameters in"
                  " files%s and %s\n", __FILE__,__LINE__,
                  bepe_file_names[i], bepe_file_names[0]);
          exit(EXIT_FAILURE);
      }
  }

  printf("file length checked: %u\n", nmodels);

  int winner;
  double temp;
  double best_val;
  char buff[2000];

  for(int i = 0; i < nfiles; ++i) { 
 	bepe_files[i] = fopen(bepe_file_names[i], "r");
  	if(bepe_files[i] == NULL) {
	fprintf(stderr,"%s:%d: can't read file \"%s\"\n",
	  __FILE__,__LINE__, bepe_file_names[i]);
 	  exit(EXIT_FAILURE); 
  	}
  	fscanf(bepe_files[i], "%s", buff);
  	fscanf(bepe_files[i], "%s", buff);
  	fscanf(bepe_files[i], "%s", buff);
  }

  for(int j = 0; j < nmodels; ++j) {
  	best_val = -1;
  	winner = -1;
  	for(int i = 0; i < nfiles; ++i) {  
  	  fscanf(bepe_files[i], "%lf", &temp);

  	  if(temp > best_val){
  	  	winner = j;
  	  	best_val = temp;
  	  }

  	  fscanf(bepe_files[i], "%s", buff);
  	  fscanf(bepe_files[i], "%s", buff);
  	  fscanf(bepe_files[i], "%s", buff);
  	  fscanf(bepe_files[i], "%s", buff);
  	}
  	winner_totals[winner]++;
  }
}
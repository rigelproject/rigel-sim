////////////////////////////////////////////////////////////////////////////////
// style_guide.cpp
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the implementation of the style guide  example for coding
//  conventions within RigelSim.
//
////////////////////////////////////////////////////////////////////////////////

// NOTE: (INCLUDE FILES) All includes must be at the top of the file.
#include "style_guide.h"
#include <iostream>

// NOTE:  (FUNCTION HEADERS) All major functions, i.e., non-accessor methods,
//        should maintain a header in the file where they are *implemented*.
//        The header should include the name, a brief description of what the
//        function is and what it returns, and parameters should be defined.
//        Date and tag any major changes at the end with [YYYYMMDD FML] (year,
//        month, day) (first, middle, last).
 
// NOTE: (FUNCTION DECLARATION) Functions should be defined using separate lines
//       for the return type, the signature, and the opening brace.

// NOTE: (LINE WRAPPING) All lines should be wrapped at 100 characters or fewer.

// NOTE: (TABBING)  There should be no tabbing.  Tabs should be two-spaces.  

////////////////////////////////////////////////////////////////////////////////
/// main()
/// 
/// This is a description of the funcion main().  Returns zero on success,
/// nonzero on failure.
/// 
/// PARAMETERS
/// int argc: Count of the parameters passed on the command line to the
///    executable.
/// char ** argv: Array of C-style strings, one string per command line
///    argument.
/// 
////////////////////////////////////////////////////////////////////////////////
int 
main(int argc, char ** argv)
{
  StyleGuide sc;

  // NOTE:  (SHORT BLOCKS) For loop and conditional constructs fewer than five
  //        lines in length, the brace should be on the same line to cut down on
  //        syntactic garbage.  Short loops should be documented at the top of
  //        the loop.

  // This loop does very little.
  for (int i = 0; i < 100; i++) {
		//NOTE: Do *NOT* use 'using namespace std', as it causes namespace pollution
		//and makes your code hard to integrate with other projects.
		//NOTE: Do not use std::endl to end lines, just use "\n".  Counterintuitively,
		//"\n" will give you the correct line ending for your platform, and the purpose
		//of std::endl is to flush the stream, which you don't always want to do for
		//performance reasons.  Only use std::endl when you explicitly do want to flush
		//the stream (e.g., if you think it's likely there will be an error afterwards)
		std::cout << "Iteration " << i << "\n";
  }

  // NOTE: (LONG BLOCKS) For long blocks, the commenting can/should be done in
  //        line.  For alignment purposes, it is also appropriate to put braces
  //        on the second line after the loop declaration.
  for (int j = 0; j < 10; j++) 
  {
    sc.add(j);
    // NOTE: (CONSTANT PREDICATES) Place constants first to avoid asignment
    //       errors.
    // NOTE: (SINGLE-LINE BLOCKS) Even for single-line blocks, use parentheses
    //       to avoid dependence on line breaks.
    if (true != sc.valid(j)) { sc.add(j); }

    for (int k = 0; k < j; k++) {
      sc.remove(k);
    }
  }
  
  // NOTE: (RETURNS) Document returns.
  // Success! 
  return 0;
}

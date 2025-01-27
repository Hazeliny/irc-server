#ifndef PASS_HPP
# define PASS_HPP

# include <iostream>
# include "ICommand.hpp"

class Pass : public ICommand
{
	public:
		~Pass( void );

		void execute( std::string );
};

#endif


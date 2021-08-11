# COUNTER
set( srcs "counter.c" )
set( include_dirs "." )
#set( requires  ) 
set( priv_requires common printfx )
idf_component_register(
	SRCS ${srcs}
	INCLUDE_DIRS ${include_dirs} 
#	REQUIRES ${requires} 
	PRIV_REQUIRES ${priv_requires}
)

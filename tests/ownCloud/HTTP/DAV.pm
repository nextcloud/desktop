#
# Perl WebDAV client library
#

package HTTP::DAV;

use LWP;
use XML::DOM;
use Time::Local;
use HTTP::DAV::Lock;
use HTTP::DAV::ResourceList;
use HTTP::DAV::Resource;
use HTTP::DAV::Comms;
use URI::file;
use URI::Escape;
use FileHandle;
use File::Glob;

#use Carp (cluck);
use Cwd ();  # Can't import all of it, cwd clashes with our namespace.

# Globals
$VERSION = '0.44';
$VERSION_DATE = '2011/06/19';

# Set this up to 3
$DEBUG = 0;

use strict;
use vars qw($VERSION $VERSION_DATE $DEBUG);

sub new {
    my $class = shift;
    my $self = bless {}, ref($class) || $class;
    $self->_init(@_);
    return $self;
}

###########################################################################
sub clone {
    my $self  = @_;
    my $class = ref($self);
    my %clone = %{$self};
    bless {%clone}, $class;
}

###########################################################################
{

    sub _init {
        my ( $self, @p ) = @_;
        my ( $uri, $headers, $useragent )
            = HTTP::DAV::Utils::rearrange( [ 'URI', 'HEADERS', 'USERAGENT' ],
            @p );

        $self->{_lockedresourcelist} = HTTP::DAV::ResourceList->new();
        $self->{_comms} = HTTP::DAV::Comms->new(
            -useragent => $useragent,
            -headers => $headers
        );
        if ($uri) {
            $self->set_workingresource( $self->new_resource( -uri => $uri ) );
        }

        return $self;
    }
}

sub DebugLevel {
    shift if ref( $_[0] ) =~ /HTTP/;
    my $level = shift;
    $level = 256 if !defined $level || $level eq "";

    $DEBUG = $level;
}

######################################################################
# new_resource acts as a resource factory.
# It will create a new one for you each time you ask.
# Sometimes, if it holds state information about this
# URL, it may return an old populated object.
sub new_resource {
    my ($self) = shift;

    ####
    # This is the order of the arguments unless used as
    # named parameters
    my ($uri) = HTTP::DAV::Utils::rearrange( ['URI'], @_ );
    $uri = HTTP::DAV::Utils::make_uri($uri);

    #cluck "new_resource: now $uri\n";

    my $resource = $self->{_lockedresourcelist}->get_member($uri);
    if ($resource) {
        print
            "new_resource: For $uri, returning existing resource $resource\n"
            if $HTTP::DAV::DEBUG > 2;

        # Just reset the url to honour trailing slash status.
        $resource->set_uri($uri);
        return $resource;
    }
    else {
        print "new_resource: For $uri, creating new resource\n"
            if $HTTP::DAV::DEBUG > 2;
        return HTTP::DAV::Resource->new(
            -Comms              => $self->{_comms},
            -LockedResourceList => $self->{_lockedresourcelist},
            -uri                => $uri,
            -Client             => $self
        );
    }
}

###########################################################################
# ACCESSOR METHODS

# GET
sub get_user_agent      { $_[0]->{_comms}->get_user_agent(); }
sub get_last_request    { $_[0]->{_comms}->get_last_request(); }
sub get_last_response   { $_[0]->{_comms}->get_last_response(); }
sub get_workingresource { $_[0]->{_workingresource} }

sub get_workingurl {
    $_[0]->{_workingresource}->get_uri()
        if defined $_[0]->{_workingresource};
}
sub get_lockedresourcelist { $_[0]->{_lockedresourcelist} }

# SET
sub set_workingresource { $_[0]->{_workingresource} = $_[1]; }
sub credentials { shift->{_comms}->credentials(@_); }

######################################################################
# Error handling

## Error conditions
my %err = (
    'ERR_WRONG_ARGS'    => 'Wrong number of arguments supplied.',
    'ERR_UNAUTHORIZED'  => 'Unauthorized. ',
    'ERR_NULL_RESOURCE' => 'Not connected. Do an open first. ',
    'ERR_RESP_FAIL'     => 'Server response: ',
    'ERR_501'           => 'Server response: ',
    'ERR_405'           => 'Server response: ',
    'ERR_GENERIC'       => '',
);

sub err {
    my ( $self, $error, $mesg, $url ) = @_;

    my $err_msg;
    $err_msg = "";
    $err_msg .= $err{$error} if defined $err{$error};
    $err_msg .= $mesg if defined $mesg;
    $err_msg .= "ERROR" unless defined $err_msg;

    $self->{_message} = $err_msg;
    my $callback = $self->{_callback};
    &$callback( 0, $err_msg, $url ) if $callback;

    if ( $self->{_multi_op} ) {
        push( @{ $self->{_errors} }, $err_msg );
    }
    $self->{_status} = 0;

    return 0;
}

sub ok {
    my ($self, $mesg, $url, $so_far, $length) = @_;

    $self->{_message} = $mesg;

    my $callback = $self->{_callback};
    &$callback(1, $mesg, $url, $so_far, $length) if $callback;

    if ($self->{_multi_op}) {
        $self->{_status} = 1 unless $self->{_status} == 0;
    }
    else {
        $self->{_status} = 1;
    }
    return 1;
}

sub _start_multi_op {
    my ($self, $mesg, $callback) = @_;
    $self->{_multi_mesg} = $mesg || "";
    $self->{_status} = 1;
    $self->{_errors} = [];
    $self->{_multi_op} = 1;
    $self->{_callback} = $callback if defined $callback;
}

sub _end_multi_op {
    my ($self) = @_;
    $self->{_multi_op} = 0;
    $self->{_callback} = undef;
    my $message = $self->{_multi_mesg} . " ";
    $message .= ( $self->{_status} ) ? "succeeded" : "failed";
    $self->{_message}    = $message;
    $self->{_multi_mesg} = undef;
}

sub message {
    my ($self) = @_;
    return $self->{_message} || "";
}

sub errors {
    my ($self) = @_;
    my $err_ref = $self->{_errors} || [];
    return @{ $err_ref };
}

sub is_success {
    my ($self) = @_;
    return $self->{_status};
}

######################################################################
# Operations

# CWD
sub cwd {
    my ( $self, @p ) = @_;
    my ($url) = HTTP::DAV::Utils::rearrange( ['URL'], @p );

    return $self->err('ERR_WRONG_ARGS') if ( !defined $url || $url eq "" );
    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    $url = HTTP::DAV::Utils::make_trail_slash($url);
    my $new_uri = $self->get_absolute_uri($url);
    ($new_uri) = $self->get_globs($new_uri);

    return 0 unless ($new_uri);

    print "cwd: Changing to $new_uri\n" if $DEBUG;
    return $self->open($new_uri);
}

# DELETE
sub delete {
    my ( $self, @p ) = @_;
    my ( $url, $callback )
        = HTTP::DAV::Utils::rearrange( [ 'URL', 'CALLBACK' ], @p );

    return $self->err('ERR_WRONG_ARGS') if ( !defined $url || $url eq "" );
    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    my $new_url = $self->get_absolute_uri($url);
    my @urls    = $self->get_globs($new_url);

    $self->_start_multi_op( "delete $url", $callback ) if @urls > 1;

    foreach my $u (@urls) {
        my $resource = $self->new_resource( -uri => $u );

        my $resp = $resource->delete();

        if ( $resp->is_success ) {
            $self->ok( "deleted $u successfully", $u );
        }
        else {
            $self->err( 'ERR_RESP_FAIL', $resp->message(), $u );
        }
    }

    $self->_end_multi_op() if @urls > 1;

    return $self->is_success;
}

# GET
# Handles globs by doing multiple recursive gets
# GET dir* produces
#   _get dir1, to_local
#   _get dir2, to_local
#   _get dir3, to_local
sub get {
    my ( $self, @p ) = @_;
    my ( $url, $to, $callback, $chunk )
        = HTTP::DAV::Utils::rearrange( [ 'URL', 'TO', 'CALLBACK', 'CHUNK' ],
        @p );

    return $self->err('ERR_WRONG_ARGS') if ( !defined $url || $url eq "" );
    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    $self->_start_multi_op( "get $url", $callback );

    my $new_url = $self->get_absolute_uri($url);
    my (@urls) = $self->get_globs($new_url);

    return 0 unless ( $#urls > -1 );

    ############
    # HANDLE -TO
    #
    $to ||= '';
    if ( $to eq '.' ) {
        $to = Cwd::getcwd();
    }

    # If the TO argument is a file handle or a scalar
    # then check that we only got one glob. If we got multiple
    # globs, then we can't keep going because we can't write multiple files
    # to one FileHandle.
    if ( $#urls > 0 ) {
        if ( ref($to) =~ /SCALAR/ ) {
            return $self->err( 'ERR_WRONG_ARGS',
                "Can't retrieve multiple files to a single scalar\n" );
        }
        elsif ( ref($to) =~ /GLOB/ ) {
            return $self->err( 'ERR_WRONG_ARGS',
                "Can't retrieve multiple files to a single filehandle\n" );
        }
    }

    # If it's a dir, remove last '/' from destination.
    # Later we need to concatenate the destination filename.
    if ( defined $to && $to ne '' && -d $to ) {
        $to =~ s{/$}{};
    }

    # Foreach file... do the get.
    foreach my $u (@urls) {
        my ( $left, $leafname ) = HTTP::DAV::Utils::split_leaf($u);

        # Handle SCALARREF and GLOB cases
        my $dest_file = $to;

        # Directories
        if ( -d $to ) {
            $dest_file = "$to/$leafname";

            # Multiple targets
        }
        elsif ( !defined $to || $to eq "" ) {
            $dest_file = $leafname;
        }

        warn "get: $u -> $dest_file\n" if $DEBUG;

        # Setup the resource based on the passed url and do a propfind.
        my $resource = $self->new_resource( -uri => $u );
        my $resp = $resource->propfind( -depth => 1 );

        if ( $resp->is_error ) {
            return $self->err( 'ERR_RESP_FAIL', $resp->message(), $u );
        }

        $self->_get( $resource, $dest_file, $callback, $chunk );
    }

    $self->_end_multi_op();
    return $self->is_success;
}

# Note: is is expected that $resource has had
# a propfind depth 1 performed on it.
#
sub _get {
    my ( $self, @p ) = @_;
    my ( $resource, $local_name, $callback, $chunk )
        = HTTP::DAV::Utils::rearrange(
        [ 'RESOURCE', 'TO', 'CALLBACK', 'CHUNK' ], @p );

    my $url = $resource->get_uri();

    # GET A DIRECTORY
    if ( $resource->is_collection ) {

        # If the TO argument is a file handle, a scalar or empty
        # then we
        # can't keep going because we can't write multiple files
        # to one FileHandle, scalar, etc.
        if ( ref($local_name) =~ /SCALAR/ ) {
            return $self->err( 'ERR_WRONG_ARGS',
                "Can't retrieve a collection to a scalar\n", $url );
        }
        elsif ( ref($local_name) =~ /GLOB/ ) {
            return $self->err( 'ERR_WRONG_ARGS',
                "Can't retrieve a collection to a filehandle\n", $url );
        }
        elsif ( $local_name eq "" ) {
            return $self->err(
                'ERR_GENERIC',
                "Can't retrieve a collection without a target directory (-to).",
                $url
            );
        }

        # Try and make the directory locally
        print "MKDIR $local_name (before escape)\n" if $DEBUG > 2;

        $local_name = URI::Escape::uri_unescape($local_name);
        if ( !mkdir $local_name ) {
            return $self->err( 'ERR_GENERIC',
                "mkdir local:$local_name failed: $!" );
        }

        $self->ok("mkdir $local_name");

        # This is the degenerate case for an empty dir.
        print "Made directory $local_name\n" if $DEBUG > 2;

        my $resource_list = $resource->get_resourcelist();
        if ($resource_list) {

            # FOREACH FILE IN COLLECTION, GET IT.
            foreach my $progeny_r ( $resource_list->get_resources() ) {

                my $progeny_url = $progeny_r->get_uri();
                print "Found progeny:$progeny_url\n" if $DEBUG > 2;
                my $progeny_local_filename
                    = HTTP::DAV::Utils::get_leafname($progeny_url);
                $progeny_local_filename
                    = URI::Escape::uri_unescape($progeny_local_filename);

                $progeny_local_filename
                    = URI::file->new($progeny_local_filename)
                    ->abs("$local_name/");

                if ( $progeny_r->is_collection() ) {
                    $progeny_r->propfind( -depth => 1 );
                }
                $self->_get( $progeny_r, $progeny_local_filename, $callback,
                    $chunk );

               # } else {
               #    $self->_do_get_tofile($progeny_r,$progeny_local_filename);
               # }
            }
        }
    }

    # GET A FILE
    else {
        my $response;
        my $name_ref = ref $local_name;

        if ( $callback || $name_ref =~ /SCALAR/ || $name_ref =~ /GLOB/ ) {
            $self->{_so_far} = 0;

            my $fh;
            my $put_to_scalar = 0;

            if ( $name_ref =~ /GLOB/ ) {
                $fh = $local_name;
            }

            elsif ( $name_ref =~ /SCALAR/ ) {
                $put_to_scalar = 1;
                $$local_name   = "";
            }

            else {
                $fh         = FileHandle->new;
                $local_name = URI::Escape::uri_unescape($local_name);
                if (! $fh->open(">$local_name") ) {
                    return $self->err(
                        'ERR_GENERIC',
                        "open \">$local_name\" failed: $!",
                        $url
                    );
                }

                # RT #29788, avoid file corruptions on Win32
                binmode $fh;
            }

            $self->{_fh} = $fh;

            $response = $resource->get(
                -chunk => $chunk,
                -progress_callback =>

                    sub {
                    my ( $data, $response, $protocol ) = @_;

                    $self->{_so_far} += length($data);

                    my $fh = $self->{_fh};
                    print $fh $data if defined $fh;

                    $$local_name .= $data if ($put_to_scalar);

                    my $user_callback = $self->{_callback};
                    &$user_callback( -1, "transfer in progress",
                        $url, $self->{_so_far}, $response->content_length(),
                        $data )
                        if defined $user_callback;

                    }

            );    # end get( ... );

            # Close the filehandle if it was set.
            if ( defined $self->{_fh} ) {
                $self->{_fh}->close();
                delete $self->{_fh};
            }
        }
        else {
            $local_name = URI::Escape::uri_unescape($local_name);
            $response = $resource->get( -save_to => $local_name );
        }

        # Handle response
        if ( $response->is_error ) {
            return $self->err( 'ERR_GENERIC',
                "get $url failed: " . $response->message, $url );
        }
        else {
            return $self->ok( "get $url", $url, $self->{_so_far},
                $response->content_length() );
        }

    }

    return 1;
}

# LOCK
sub lock {
    my ( $self, @p ) = @_;
    my ( $url, $owner, $depth, $timeout, $scope, $type, @other )
        = HTTP::DAV::Utils::rearrange(
        [ 'URL', 'OWNER', 'DEPTH', 'TIMEOUT', 'SCOPE', 'TYPE' ], @p );

    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    my $resource;
    if ($url) {
        $url = $self->get_absolute_uri($url);
        $resource = $self->new_resource( -uri => $url );
    }
    else {
        $resource = $self->get_workingresource();
        $url      = $resource->get_uri;
    }

    # Make the lock
    my $resp = $resource->lock(
        -owner   => $owner,
        -depth   => $depth,
        -timeout => $timeout,
        -scope   => $scope,
        -type    => $type
    );

    if ( $resp->is_success() ) {
        return $self->ok( "lock $url succeeded", $url );
    }
    else {
        return $self->err( 'ERR_RESP_FAIL', $resp->message, $url );
    }
}

# UNLOCK
sub unlock {
    my ( $self, @p ) = @_;
    my ($url) = HTTP::DAV::Utils::rearrange( ['URL'], @p );

    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    my $resource;
    if ($url) {
        $url = $self->get_absolute_uri($url);
        $resource = $self->new_resource( -uri => $url );
    }
    else {
        $resource = $self->get_workingresource();
        $url      = $resource->get_uri;
    }

    # Make the lock
    my $resp = $resource->unlock();
    if ( $resp->is_success ) {
        return $self->ok( "unlock $url succeeded", $url );
    }
    else {

        # The Resource.pm::lock routine has a hack
        # where if it doesn't know the locktoken, it will
        # just return an empty response with message "Client Error".
        # Make a custom message for this case.
        my $msg = $resp->message;
        if ( $msg =~ /Client error/i ) {
            $msg = "No locks found. Try steal";
            return $self->err( 'ERR_GENERIC', $msg, $url );
        }
        else {
            return $self->err( 'ERR_RESP_FAIL', $msg, $url );
        }
    }
}

sub steal {
    my ( $self, @p ) = @_;
    my ($url) = HTTP::DAV::Utils::rearrange( ['URL'], @p );

    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    my $resource;
    if ($url) {
        $url = $self->get_absolute_uri($url);
        $resource = $self->new_resource( -uri => $url );
    }
    else {
        $resource = $self->get_workingresource();
    }

    # Go the steal
    my $resp = $resource->forcefully_unlock_all();
    if ( $resp->is_success() ) {
        return $self->ok( "steal succeeded", $url );
    }
    else {
        return $self->err( 'ERR_RESP_FAIL', $resp->message(), $url );
    }
}

# MKCOL
sub mkcol {
    my ( $self, @p ) = @_;
    my ($url) = HTTP::DAV::Utils::rearrange( ['URL'], @p );

    return $self->err('ERR_WRONG_ARGS') if ( !defined $url || $url eq "" );
    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    $url = HTTP::DAV::Utils::make_trail_slash($url);
    my $new_url = $self->get_absolute_uri($url);
    my $resource = $self->new_resource( -uri => $new_url );

    # Make the lock
    my $resp = $resource->mkcol();
    if ( $resp->is_success() ) {
        return $self->ok( "mkcol $new_url", $new_url );
    }
    else {
        return $self->err( 'ERR_RESP_FAIL', $resp->message(), $new_url );
    }
}

# OPTIONS
sub options {
    my ( $self, @p ) = @_;
    my ($url) = HTTP::DAV::Utils::rearrange( ['URL'], @p );

    #return $self->err('ERR_WRONG_ARGS') if (!defined $url || $url eq "");
    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    my $resource;
    if ($url) {
        $url = $self->get_absolute_uri($url);
        $resource = $self->new_resource( -uri => $url );
    }
    else {
        $resource = $self->get_workingresource();
        $url      = $resource->get_uri;
    }

    # Make the call
    my $resp = $resource->options();
    if ( $resp->is_success() ) {
        $self->ok( "options $url succeeded", $url );
        return $resource->get_options();
    }
    else {
        $self->err( 'ERR_RESP_FAIL', $resp->message(), $url );
        return undef;
    }
}

# MOVE
sub move { return shift->_move_copy( "move", @_ ); }
sub copy { return shift->_move_copy( "copy", @_ ); }

sub _move_copy {
    my ( $self, $method, @p ) = @_;
    my ( $url, $dest_url, $overwrite, $depth, $text, @other )
        = HTTP::DAV::Utils::rearrange(
        [ 'URL', 'DEST', 'OVERWRITE', 'DEPTH', 'TEXT' ], @p );

    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    if (!(  defined $url && $url ne "" && defined $dest_url && $dest_url ne ""
        )
        )
    {
        return $self->err( 'ERR_WRONG_ARGS',
            "Must supply a source and destination url" );
    }

    $url      = $self->get_absolute_uri($url);
    $dest_url = $self->get_absolute_uri($dest_url);
    my $resource      = $self->new_resource( -uri => $url );
    my $dest_resource = $self->new_resource( -uri => $dest_url );

    my $resp = $dest_resource->propfind( -depth => 1 );
    if ( $resp->is_success && $dest_resource->is_collection ) {
        my $leafname = HTTP::DAV::Utils::get_leafname($url);
        $dest_url = "$dest_url/$leafname";
        $dest_resource = $self->new_resource( -uri => $dest_url );
    }

    # Make the lock
    $resp = $resource->$method(
        -dest      => $dest_resource,
        -overwrite => $overwrite,
        -depth     => $depth,
        -text      => $text,
    );

    if ( $resp->is_success() ) {
        return $self->ok( "$method $url to $dest_url succeeded", $url );
    }
    else {
        return $self->err( 'ERR_RESP_FAIL', $resp->message, $url );
    }
}

# OPEN
# Must be a collection resource
# $dav->open( -url => http://localhost/test/ );
# $dav->open( localhost/test/ );
# $dav->open( -url => localhost:81 );
# $dav->open( localhost );
sub open {
    my ( $self, @p ) = @_;
    my ($url) = HTTP::DAV::Utils::rearrange( ['URL'], @p );

    my $resource;
    if ( defined $url && $url ne "" ) {
        $url = HTTP::DAV::Utils::make_trail_slash($url);
        $resource = $self->new_resource( -uri => $url );
    }
    else {
        $resource = $self->get_workingresource();
        $url = $resource->get_uri() if ($resource);
        return $self->err('ERR_WRONG_ARGS')
            if ( !defined $url || $url eq "" );
    }

    my $response = $resource->propfind( -depth => 0 );
    #print $response->as_string;
    # print $resource->as_string;

    my $result = $self->what_happened($url, $resource, $response);
    if ($result->{success} == 0) {
        return $self->err($result->{error_type}, $result->{error_msg}, $url);
    }

    # If it is a collection but the URI doesn't end in a trailing slash.
    # Then we need to reopen with the /
    elsif ($resource->is_collection
        && $url !~ m#/\s*$# )
    {
        my $newurl = $url . "/";
        print "Redirecting to $newurl\n" if $DEBUG > 1;
        return $self->open($newurl);
    }

    # If it is not a collection then we
    # can't open it.
   # elsif ( !$resource->is_collection ) {
   #     return $self->err( 'ERR_GENERIC',
   #         "Operation failed. You can only open a collection (directory)",
   #         $url );
   # }
    else {
        $self->set_workingresource($resource);
        return $self->ok( "Connected to $url", $url );
    }

    return $self->err( 'ERR_GENERIC', $url );
}

# Performs a propfind and then returns the populated
# resource. The resource will have a resourcelist if
# it is a collection.
sub propfind {
    my ( $self, @p ) = @_;
    my ( $url, $depth ) = HTTP::DAV::Utils::rearrange( [ 'URL', 'DEPTH' ], @p );

    # depth = 1 is the default
    if (! defined $depth) {
        $depth = 1;
    }

    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    my $resource;
    if ($url) {
        $url = $self->get_absolute_uri($url);
        $resource = $self->new_resource( -uri => $url );
    }
    else {
        $resource = $self->get_workingresource();
    }

    # Make the call
    my $resp = $resource->propfind( -depth => $depth );
    if ( $resp->is_success() ) {
        $resource->build_ls($resource);
        $self->ok( "propfind " . $resource->get_uri() . " succeeded", $url );
        return $resource;
    }
    else {
        return $self->err( 'ERR_RESP_FAIL', $resp->message(), $url );
    }
}

# Set a property on the resource
sub set_prop {
    my ( $self, @p ) = @_;
    my ( $url, $namespace, $propname, $propvalue, $nsabbr )
        = HTTP::DAV::Utils::rearrange(
        [ 'URL', 'NAMESPACE', 'PROPNAME', 'PROPVALUE', 'NSABBR' ], @p );
    $self->proppatch(
        -url       => $url,
        -namespace => $namespace,
        -propname  => $propname,
        -propvalue => $propvalue,
        -action    => "set",
        -nsabbr    => $nsabbr,
    );
}

# Unsets a property on the resource
sub unset_prop {
    my ( $self, @p ) = @_;
    my ( $url, $namespace, $propname, $nsabbr )
        = HTTP::DAV::Utils::rearrange(
        [ 'URL', 'NAMESPACE', 'PROPNAME', 'NSABBR' ], @p );
    $self->proppatch(
        -url       => $url,
        -namespace => $namespace,
        -propname  => $propname,
        -action    => "remove",
        -nsabbr    => $nsabbr,
    );
}

# Performs a proppatch on the resource
sub proppatch {
    my ( $self, @p ) = @_;
    my ( $url, $namespace, $propname, $propvalue, $action, $nsabbr )
        = HTTP::DAV::Utils::rearrange(
        [ 'URL', 'NAMESPACE', 'PROPNAME', 'PROPVALUE', 'ACTION', 'NSABBR' ],
        @p );

    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    my $resource;
    if ($url) {
        $url = $self->get_absolute_uri($url);
        $resource = $self->new_resource( -uri => $url );
    }
    else {
        $resource = $self->get_workingresource();
    }

    # Make the call
    my $resp = $resource->proppatch(
        -namespace => $namespace,
        -propname  => $propname,
        -propvalue => $propvalue,
        -action    => $action,
        -nsabbr    => $nsabbr
    );

    if ( $resp->is_success() ) {
        $resource->build_ls($resource);
        $self->ok( "proppatch " . $resource->get_uri() . " succeeded", $url );
        return $resource;
    }
    else {
        return $self->err( 'ERR_RESP_FAIL', $resp->message(), $url );
    }
}

######################################################################
sub put {
    my ( $self, @p ) = @_;
    my ( $local, $url, $callback, $custom_headers )
        = HTTP::DAV::Utils::rearrange( [ 'LOCAL', 'URL', 'CALLBACK', 'HEADERS' ], @p );

    if ( ref($local) eq "SCALAR" ) {
    	$self->_start_multi_op( 'put ' . ${$local}, $callback );
        $self->_put(@p);
    }
    else {
    	$self->_start_multi_op( 'put ' . $local, $callback );
        $local =~ s/\ /\\ /g;
        my @globs = glob("$local");
        foreach my $file (@globs) {
            print "Starting put of $file\n" if $HTTP::DAV::DEBUG > 1;
            $self->_put(
                -local    => $file,
                -url      => $url,
                -callback => $callback,
                -headers  => $custom_headers,
            );
        }
    }
    $self->_end_multi_op();
    return $self->is_success;
}

sub _put {
    my ( $self, @p ) = @_;
    my ( $local, $url, $custom_headers )
        = HTTP::DAV::Utils::rearrange( [ 'LOCAL', 'URL', 'HEADERS' ], @p );

    return $self->err('ERR_WRONG_ARGS')
        if ( !defined $local || $local eq "" );
    return $self->err('ERR_NULL_RESOURCE')
        unless $self->get_workingresource();

    # Check if they passed a reference to content rather than a filename.
    my $content_ptr = ( ref($local) eq "SCALAR" ) ? 1 : 0;

    # Setup the resource based on the passed url
    # Check if the remote resource exists and is a collection.
    $url = $self->get_absolute_uri($url);
    my $resource = $self->new_resource($url);
    my $response = $resource->propfind( -depth => 0 );
    my $leaf_name;
    if ( $response->is_success && $resource->is_collection && !$content_ptr )
    {

        # Add one / to the end of the collection
        $url =~ s/\/*$//g;    #Strip em
        $url .= "/";          #Add one
        $leaf_name = HTTP::DAV::Utils::get_leafname($local);
    }
    else {
        $leaf_name = HTTP::DAV::Utils::get_leafname($url);
    }

    my $target = $self->get_absolute_uri( $leaf_name, $url );

    print "$local => $target ($url, $leaf_name)\n";

    # PUT A DIRECTORY
    if ( !$content_ptr && -d $local ) {

        # mkcol
        # Return 0 if fail because the error will have already
        # been set by the mkcol routine
        if ( $self->mkcol($target, -headers => $custom_headers) ) {
            if ( !opendir( DIR, $local ) ) {
                $self->err( 'ERR_GENERIC', "chdir to \"$local\" failed: $!" );
            }
            else {
                my @files = readdir(DIR);
                close DIR;
                foreach my $file (@files) {
                    next if $file eq ".";
                    next if $file eq "..";
                    my $progeny = "$local/$file";
                    $progeny =~ s#//#/#g;    # Fold down double slashes
                    $self->_put(
                        -local => $progeny,
                        -url   => "$target/$file",
                    );
                }
            }
        }

    # PUT A FILE
    }
    else {
        my $content = "";
        my $fail    = 0;
        if ($content_ptr) {
            $content = $$local;
        }
        else {
            if ( !CORE::open( F, $local ) ) {
                $self->err( 'ERR_GENERIC',
                    "Couldn't open local file $local: $!" );
                $fail = 1;
            }
            else {
                binmode F;
                while (<F>) { $content .= $_; }
                close F;
            }
        }

        if ( !$fail ) {
            my $resource = $self->new_resource( -uri => $target );
            my $response = $resource->put($content,$custom_headers);
            if ( $response->is_success ) {
                $self->ok( "put $target (" . length($content) . " bytes)",
                    $target );
            }
            else {
                $self->err( 'ERR_RESP_FAIL',
                    "put failed " . $response->message(), $target );
            }
        }
    }
}

######################################################################
# UTILITY FUNCTION
# get_absolute_uri:
# Synopsis: $new_url = get_absolute_uri("/foo/bar")
# Takes a URI (or string)
# and returns the absolute URI based
# on the remote current working directory
sub get_absolute_uri {
    my ( $self, @p ) = @_;
    my ( $rel_uri, $base_uri )
        = HTTP::DAV::Utils::rearrange( [ 'REL_URI', 'BASE_URI' ], @p );

    local $URI::URL::ABS_REMOTE_LEADING_DOTS = 1;
    if ( !defined $base_uri ) {
        $base_uri = $self->get_workingresource()->get_uri();
    }

    if ($base_uri) {
        my $new_url = URI->new_abs( $rel_uri, $base_uri );
        return $new_url;
    }
    else {
        $rel_uri;
    }
}

## Takes a $dav->get_globs(URI)
# Where URI may contain wildcards at the leaf level:
# URI:
#   http://www.host.org/perldav/test*.html
#   /perldav/test?.html
#   test[12].html
#
# Performs a propfind to determine the url's that match
#
sub get_globs {
    my ( $self, $url ) = @_;
    my @urls = ();
    my ( $left, $leafname ) = HTTP::DAV::Utils::split_leaf($url);

    # We need to unescape it because it may have been encoded.
    $leafname = URI::Escape::uri_unescape($leafname);

    if ( $leafname =~ /[\*\?\[]/ ) {
        my $resource = $self->new_resource( -uri => $left );
        my $resp = $resource->propfind( -depth => 1 );
        if ( $resp->is_error ) {
            $self->err( 'ERR_RESP_FAIL', $resp->message(), $left );
            return ();
        }

        $leafname = HTTP::DAV::Utils::glob2regex($leafname);
        my $rl = $resource->get_resourcelist();
        if ($rl) {
            my $match = 0;

            # We eval this because a bogus leafname could bomb the regex.
            eval {
                foreach my $progeny ( $rl->get_resources() )
                {
                    my $progeny_url = $progeny->get_uri;
                    my $progeny_leaf
                        = HTTP::DAV::Utils::get_leafname($progeny_url);
                    if ( $progeny_leaf =~ /^$leafname$/ ) {
                        print "Matched $progeny_url\n"
                            if $HTTP::DAV::DEBUG > 1;
                        $match++;
                        push( @urls, $progeny_url );
                    }
                    else {
                        print "Skipped $progeny_url\n"
                            if $HTTP::DAV::DEBUG > 1;
                    }
                }
            };
            $self->err( 'ERR_GENERIC', "No match found" ) unless ($match);
        }
    }
    else {
        push( @urls, $url );
    }

    return @urls;
}

sub what_happened {
    my ($self, $url, $resource, $response) = @_;

    if (! $response->is_error()) {
        return { success => 1 }
    }

    my $error_type;
    my $error_msg;

    # Method not allowed
    if ($response->status_line =~ m{405}) {
        $error_type = 'ERR_405';
        $error_msg = $response->status_line;
    }
    # 501 most probably means your LWP doesn't support SSL
    elsif ($response->status_line =~ m{501}) {
        $error_type = 'ERR_501';
        $error_msg = $response->status_line;
    }
    elsif ($response->www_authenticate) {
        $error_type = 'ERR_UNAUTHORIZED';
        $error_msg  = $response->www_authenticate;
    }
    elsif ( !$resource->is_dav_compliant ) {
        $error_type = 'ERR_GENERIC';
        $error_msg = qq{The URL "$url" is not DAV enabled or not accessible.};
    }
    else {
        $error_type = 'ERR_RESP_FAIL';
        my $message = $response->message();
        $error_msg = qq{Could not access $url: $message};
    }

    return {
        success => 0,
        error_type => $error_type,
        error_msg => $error_msg,
    }

}

1;

__END__


=head1 NAME

HTTP::DAV - A WebDAV client library for Perl5

=head1 SYNOPSIS

   # DAV script that connects to a webserver, safely makes 
   # a new directory and uploads all html files in 
   # the /tmp directory.

   use HTTP::DAV;
  
   $d = HTTP::DAV->new();
   $url = "http://host.org:8080/dav/";
 
   $d->credentials(
      -user  => "pcollins",
      -pass  => "mypass", 
      -url   => $url,
      -realm => "DAV Realm"
   );
 
   $d->open( -url => $url )
      or die("Couldn't open $url: " .$d->message . "\n");
 
   # Make a null lock on newdir
   $d->lock( -url => "$url/newdir", -timeout => "10m" ) 
      or die "Won't put unless I can lock for 10 minutes\n";

   # Make a new directory
   $d->mkcol( -url => "$url/newdir" )
      or die "Couldn't make newdir at $url\n";
  
   # Upload multiple files to newdir.
   if ( $d->put( -local => "/tmp/*.html", -url => $url ) ) {
      print "successfully uploaded multiple files to $url\n";
   } else {
      print "put failed: " . $d->message . "\n";
   }
  
   $d->unlock( -url => $url );

=head1 DESCRIPTION

HTTP::DAV is a Perl API for interacting with and modifying content on webservers using the WebDAV protocol. Now you can LOCK, DELETE and PUT files and much more on a DAV-enabled webserver.

HTTP::DAV is part of the PerlDAV project hosted at http://www.webdav.org/perldav/ and has the following features:

=over 4

=item *

Full RFC2518 method support. OPTIONS, TRACE, GET, HEAD, DELETE, PUT, COPY, MOVE, PROPFIND, PROPPATCH, LOCK, UNLOCK.

=item *

A fully object-oriented API.

=item *

Recursive GET and PUT for site backups and other scripted transfers.

=item *

Transparent lock handling when performing LOCK/COPY/UNLOCK sequences.

=item *

http and https support (https requires the Crypt::SSLeay library). See INSTALLATION.

=item *

Basic AND Digest authentication support (Digest auth requires the MD5 library). See INSTALLATION.

=item *

C<dave>, a fully-functional ftp-style interface written on top of the HTTP::DAV API and bundled by default with the HTTP::DAV library. (If you've already installed HTTP::DAV, then dave will also have been installed (probably into /usr/local/bin). You can see it's man page by typing "perldoc dave" or going to http://www.webdav.org/perldav/dave/.

=item *

It is built on top of the popular LWP (Library for WWW access in Perl). This means that HTTP::DAV inherits proxy support, redirect handling, basic (and digest) authorization and many other HTTP operations. See C<LWP> for more information.

=item *

Popular server support. HTTP::DAV has been tested against the following servers: mod_dav, IIS5, Xythos webfile server and mydocsonline. The library is growing an impressive interoperability suite which also serves as useful "sample scripts". See "make test" and t/*.

=back

C<HTTP::DAV> essentially has two API's, one which is accessed through this module directly (HTTP::DAV) and is a simple abstraction to the rest of the HTTP::DAV::* Classes. The other interface consists of the HTTP::DAV::* classes which if required allow you to get "down and dirty" with your DAV and HTTP interactions.

The methods provided in C<HTTP::DAV> should do most of what you want. If, however, you need more control over the client's operations or need more info about the server's responses then you will need to understand the rest of the HTTP::DAV::* interfaces. A good place to start is with the C<HTTP::DAV::Resource> and C<HTTP::DAV::Response> documentation.

=head1 METHODS

=head2 METHOD CALLING: Named vs Unnamed parameters

You can pass parameters to C<HTTP::DAV> methods in one of two ways: named or unnamed.

Named parameters provides for a simpler/easier to use interface. A named interface affords more readability and allows the developer to ignore a specific order on the parameters. (named parameters are also case insensitive) 

Each argument name is preceded by a dash.  Neither case nor order matters in the argument list.  -url, -Url, and -URL are all acceptable.  In fact, only the first argument needs to begin with a dash.  If a dash is present in the first argument, C<HTTP::DAV> assumes dashes for the subsequent ones.

Each method can also be called with unnamed parameters which often makes sense for methods with only one parameter. But the developer will need to ensure that the parameters are passed in the correct order (as listed in the docs).

 Doc:     method( -url=>$url, [-depth=>$depth] )
 Named:   $d->method( -url=>$url, -depth=>$d ); # VALID
 Named:   $d->method( -Depth=>$d, -Url=>$url ); # VALID
 Named:   $d->method( Depth=>$d,  Url=>$url );  # INVALID (needs -)
 Named:   $d->method( -Arg2=>$val2 ); # INVALID, ARG1 is not optional
 Unnamed: $d->method( $val1 );        # VALID
 Unnamed: $d->method( $val2,$val1 );  # INVALID, ARG1 must come first.

IMPORTANT POINT!!!! If you specify a named parameter first but then forget for the second and third parameters, you WILL get weird things happen. E.g. this is bad:

 $d->method( -url=>$url, $arg2, $arg3 ); # BAD BAD BAD

=head2 THINGS YOU NEED TO KNOW

In all of the methods specified in L<PUBLIC METHODS> there are some common concepts you'll need to understand:

=over 4

=item * URLs represent an absolute or relative URI. 

  -url=>"host.org/dav_dir/"  # Absolute
  -url=>"/dav_dir/"          # Relative
  -url=>"file.txt"           # Relative

You can only use a relative URL if you have already "open"ed an absolute URL.

The HTTP::DAV module now consistently uses the named parameter: URL. The lower-level HTTP::DAV::Resource interface inconsistently interchanges URL and URI. I'm working to resolve this, in the meantime, you'll just need to remember to use the right one by checking the documentation if you need to mix up your use of both interfaces.

=item * GLOBS

Some methods accept wildcards in the URL. A wildcard can be used to indicate that the command should perform the command on all Resources that match the wildcard. These wildcards are called GLOBS.

The glob may contain the characters "*", "?" and the set operator "[...]" where ... contains multiple characters ([1t2]) or a range such ([1-5]). For the curious, the glob is converted to a regex and then matched: "*" to ".*", "?" to ".", and the [] is left untouched.

It is important to note that globs only operate at the leaf-level. For instance "/my_dir/*/file.txt" is not a valid glob.

If a glob matches no URL's the command will fail (which normally means returns 0).

Globs are useful in conjunction with L<CALLBACKS> to provide feedback as each operation completes.

See the documentation for each method to determine whether it supports globbing.

Globs are useful for interactive style applications (see the source code for C<dave> as an example).

Example globs:

   $dav1->delete(-url=>"/my_dir/file[1-3]");     # Matches file1, file2, file3
   $dav1->delete(-url=>"/my_dir/file[1-3]*.txt");# Matches file1*.txt,file2*.txt,file3*.txt
   $dav1->delete(-url=>"/my_dir/*/file.txt");    # Invalid. Can only match at leaf-level

=item * CALLBACKS

Callbacks are used by some methods (primarily get and put) to give the caller some insight as to how the operation is progressing. A callback allows you to define a subroutine as defined below and pass a reference (\&ref) to the method.

The rationale behind the callback is that a recursive get/put or an operation against many files (using a C<glob>) can actually take a long time to complete.

Example callback:

   $d->get( -url=>$url, -to=>$to, -callback=>\&mycallback );

Your callback function MUST accept arguments as follows:
   sub cat_callback {
      my($status,$mesg,$url,$so_far,$length,$data) = @_;
      ...
   }

The C<status> argument specifies whether the operation has succeeded (1), failed (0), or is in progress (-1).

The C<mesg> argument is a status message. The status message could contain any string and often contains useful error messages or success messages. 

The C<url> the remote URL.

The C<so_far>, C<length> - these parameters indicate how many bytes have been downloaded and how many we should expect. This is useful for doing "56% to go" style-gauges. 

The C<data> parameter - is the actual data transferred. The C<cat> command uses this to print the data to the screen. This value will be empty for C<put>.

See the source code of C<dave> for a useful sample of how to setup a callback.

Note that these arguments are NOT named parameters.

All error messages set during a "multi-operation" request (for instance a recursive get/put) are also retrievable via the C<errors()> function once the operation has completed. See C<ERROR HANDLING> for more information.

=back

=head2 PUBLIC METHODS

=over 4

=item B<new(USERAGENT)>

=item B<new(USERAGENT, HEADERS)>

Creates a new C<HTTP::DAV> client

 $d = HTTP::DAV->new();

The C<-useragent> parameter allows you to pass your own B<user agent object> and expects an C<HTTP::DAV::UserAgent> object. See the C<dave> program for an advanced example of a custom UserAgent that interactively prompts the user for their username and password.

The C<-headers> parameter allows you to specify a list of headers to be sent along with all requests. This can be either a hashref like:

  { "X-My-Header" => "value", ... }

or a L<HTTP::Headers> object.

=item B<credentials(USER,PASS,[URL],[REALM])>

sets authorization credentials for a C<URL> and/or C<REALM>.

When the client hits a protected resource it will check these credentials to see if either the C<URL> or C<REALM> match the authorization response.

Either C<URL> or C<REALM> must be provided.

returns no value

Example:

 $d->credentials( -url=>'myhost.org:8080/test/',
                  -user=>'pcollins',
                  -pass=>'mypass');

=item B<DebugLevel($val)>

sets the debug level to C<$val>. 0=off 3=noisy.

C<$val> default is 0. 

returns no value.

When the value is greater than 1, the C<HTTP::DAV::Comms> module will log all of the client<=>server interactions into /tmp/perldav_debug.txt.

=back

=head2 DAV OPERATIONS

For all of the following operations, URL can be absolute (http://host.org/dav/) or relative (../dir2/). The only operation that requires an absolute URL is open.

=over 4 

=item B<copy(URL,DEST,[OVERWRITE],[DEPTH])>

copies one remote resource to another

=over 4 

=item C<-url> 

is the remote resource you'd like to copy. Mandatory

=item C<-dest> 

is the remote target for the copy command. Mandatory

=item C<-overwrite> 

optionally indicates whether the server should fail if the target exists. Valid values are "T" and "F" (1 and 0 are synonymous). Default is T.

=item C<-depth> 

optionally indicates whether the server should do a recursive copy or not. Valid values are 0 and (1 or "infinity"). Default is "infinity" (1).

=back

The return value is always 1 or 0 indicating success or failure.

Requires a working resource to be set before being called. See C<open>.

Note: if either C<'URL'> or C<'DEST'> are locked by this dav client, then the lock headers will be taken care of automatically. If the either of the two URL's are locked by someone else, the server should reject the request.

B<copy examples:>

  $d->open(-url=>"host.org/dav_dir/");

Recursively copy dir1/ to dir2/

  $d->copy(-url=>"dir1/", -dest=>"dir2/");

Non-recursively and non-forcefully copy dir1/ to dir2/

  $d->copy(-url=>"dir1/", -dest=>"dir2/",-overwrite=>0,-depth=>0);

Create a copy of dir1/file.txt as dir2/file.txt

  $d->cwd(-url=>"dir1/");
  $d->copy("file.txt","../dir2");

Create a copy of file.txt as dir2/new_file.txt

  $d->copy("file.txt","/dav_dir/dir2/new_file.txt")

=item B<cwd(URL)>

changes the remote working directory. 

This is synonymous to open except that the URL can be relative and may contain a C<glob> (the first match in a glob will be used).

  $d->open("host.org/dav_dir/dir1/");
  $d->cwd("../dir2");
  $d->cwd(-url=>"../dir1");

The return value is always 1 or 0 indicating success or failure. 

Requires a working resource to be set before being called. See C<open>.

You can not cwd to files, only collections (directories).

=item B<delete(URL)>

deletes a remote resource.

  $d->open("host.org/dav_dir/");
  $d->delete("index.html");
  $d->delete("./dir1");
  $d->delete(-url=>"/dav_dir/dir2/file*",-callback=>\&mycallback);

=item C<-url>

is the remote resource(s) you'd like to delete. It can be a file, directory or C<glob>. 

=item C<-callback>                                                                                                                                                                    is a reference to a callback function which will be called everytime a file is deleted. This is mainly useful when used in conjunction with L<GLOBS> deletes. See L<callbacks>

The return value is always 1 or 0 indicating success or failure. 

Requires a working resource to be set before being called. See C<open>.

This command will recursively delete directories. BE CAREFUL of uninitialised file variables in situation like this: $d->delete("$dir/$file"). This will trash your $dir if $file is not set.

=item B<get(URL,[TO],[CALLBACK])>

downloads the file or directory at C<URL> to the local location indicated by C<TO>.

=over 4 

=item C<-url> 

is the remote resource you'd like to get. It can be a file or directory or a "glob".

=item C<-to> 

is where you'd like to put the remote resource. The -to parameter can be:

 - a B<filename> indicating where to save the contents.

 - a B<FileHandle reference>.

 - a reference to a B<scalar object> into which the contents will be saved.

If the C<-url> matches multiple files (via a glob or a directory download), then the C<get> routine will return an error if you try to use a FileHandle reference or a scalar reference.

=item C<-callback>

is a reference to a callback function which will be called everytime a file is completed downloading. The idea of the callback function is that some recursive get's can take a very long time and the user may require some visual feedback. See L<CALLBACKS> for an examples and how to use a callback.

=back

The return value of get is always 1 or 0 indicating whether the entire get sequence was a success or if there was ANY failures. For instance, in a recursive get, if the server couldn't open 1 of the 10 remote files, for whatever reason, then the return value will be 0. This is so that you can have your script call the C<errors()> routine to handle error conditions.

Previous versions of HTTP::DAV allowed the return value to be the file contents if no -to attribute was supplied. This functionality is deprecated.

Requires a working resource to be set before being called. See C<open>.

B<get examples:>

  $d->open("host.org/dav_dir/");

Recursively get remote my_dir/ to .

  $d->get("my_dir/",".");

Recursively get remote my_dir/ to /tmp/my_dir/ calling &mycallback($success,$mesg) everytime a file operation is completed.

  $d->get("my_dir","/tmp",\&mycallback);

Get remote my_dir/index.html to /tmp/index.html

  $d->get(-url=>"/dav_dir/my_dir/index.html",-to=>"/tmp");

Get remote index.html to /tmp/index1.html

  $d->get("index.html","/tmp/index1.html");

Get remote index.html to a filehandle

  my $fh = new FileHandle;
  $fh->open(">/tmp/index1.html");
  $d->get("index.html",\$fh);

Get remote index.html as a scalar (into the string $file_contents):

  my $file_contents;
  $d->get("index.html",\$file_contents);

Get all of the files matching the globs file1* and file2*:

  $d->get("file[12]*","/tmp");

Get all of the files matching the glob file?.html:

  $d->get("file?.html","/tmp"); # downloads file1.html and file2.html but not file3.html or file1.txt

Invalid glob:

  $d->get("/dav_dir/*/index.html","/tmp"); # Can not glob like this.

=item B<lock([URL],[OWNER],[DEPTH],[TIMEOUT],[SCOPE],[TYPE])>

locks a resource. If URL is not specified, it will lock the current working resource (opened resource).

   $d->lock( -url     => "index.html",
             -owner   => "Patrick Collins",
             -depth   => "infinity",
             -scope   => "exclusive",
             -type    => "write",
             -timeout => "10h" )

See C<HTTP::DAV::Resource> lock() for details of the above parameters.

The return value is always 1 or 0 indicating success or failure. 

Requires a working resource to be set before being called. See C<open>.

When you lock a resource, the lock is held against the current HTTP::DAV object. In fact, the locks are held in a C<HTTP::DAV::ResourceList> object. You can operate against all of the locks that you have created as follows:

  ## Print and unlock all locks that we own.
  my $rl_obj = $d->get_lockedresourcelist();
  foreach $resource ( $rl_obj->get_resources() ) {
      @locks = $resource->get_locks(-owned=>1);
      foreach $lock ( @locks ) { 
        print $resource->get_uri . "\n";
        print $lock->as_string . "\n";
      }
      ## Unlock them?
      $resource->unlock;
  }

Typically, a simple $d->unlock($uri) will suffice.

B<lock example>

  $d->lock($uri, -timeout=>"1d");
  ...
  $d->put("/tmp/index.html",$uri);
  $d->unlock($uri);

=item B<mkcol(URL)>

make a remote collection (directory)

The return value is always 1 or 0 indicating success or failure. 

Requires a working resource to be set before being called. See C<open>.

  $d->open("host.org/dav_dir/");
  $d->mkcol("new_dir");                  # Should succeed
  $d->mkcol("/dav_dir/new_dir");         # Should succeed
  $d->mkcol("/dav_dir/new_dir/xxx/yyy"); # Should fail

=item B<move(URL,DEST,[OVERWRITE],[DEPTH])>

moves one remote resource to another

=over 4 

=item C<-url> 

is the remote resource you'd like to move. Mandatory

=item C<-dest> 

is the remote target for the move command. Mandatory

=item C<-overwrite> 

optionally indicates whether the server should fail if the target exists. Valid values are "T" and "F" (1 and 0 are synonymous). Default is T.

=back

Requires a working resource to be set before being called. See C<open>.

The return value is always 1 or 0 indicating success or failure.

Note: if either C<'URL'> or C<'DEST'> are locked by this dav client, then the lock headers will be taken care of automatically. If either of the two URL's are locked by someone else, the server should reject the request.

B<move examples:>

  $d->open(-url=>"host.org/dav_dir/");

move dir1/ to dir2/

  $d->move(-url=>"dir1/", -dest=>"dir2/");

non-forcefully move dir1/ to dir2/

  $d->move(-url=>"dir1/", -dest=>"dir2/",-overwrite=>0);

Move dir1/file.txt to dir2/file.txt

  $d->cwd(-url=>"dir1/");
  $d->move("file.txt","../dir2");

move file.txt to dir2/new_file.txt

  $d->move("file.txt","/dav_dir/dir2/new_file.txt")

=item B<open(URL)>

opens the directory (collection resource) at URL.

open will perform a propfind against URL. If the server does not understand the request then the open will fail. 

Similarly, if the server indicates that the resource at URL is NOT a collection, the open command will fail.

=item B<options([URL])>

Performs an OPTIONS request against the URL or the working resource if URL is not supplied.

Requires a working resource to be set before being called. See C<open>.

The return value is a string of comma separated OPTIONS that the server states are legal for URL or undef otherwise.

A fully compliant DAV server may offer as many methods as: OPTIONS, TRACE, GET, HEAD, DELETE, PUT, COPY, MOVE, PROPFIND, PROPPATCH, LOCK, UNLOCK

Note: IIS5 does not support PROPPATCH or LOCK on collections.

Example:

 $options = $d->options($url);
 print $options . "\n";
 if ($options=~ /\bPROPPATCH\b/) {
    print "OK to proppatch\n";
 }

Or, put more simply:

 if ( $d->options($url) =~ /\bPROPPATCH\b/ ) {
    print "OK to proppatch\n";
 }

=item B<propfind([URL],[DEPTH])>

Perform a propfind against URL at DEPTH depth.

C<-depth> can be used to specify how deep the propfind goes. "0" is collection only. "1" is collection and it's immediate members (This is the default value). "infinity" is the entire directory tree. Note that most DAV compliant servers deny "infinity" depth propfinds for security reasons.

Requires a working resource to be set before being called. See C<open>.

The return value is an C<HTTP::DAV::Resource> object on success or 0 on failure.

The Resource object can be used for interrogating properties or performing other operations.

 ## Print collection or content length
 if ( $r=$d->propfind( -url=>"/my_dir", -depth=>1) ) {
    if ( $r->is_collection ) {
       print "Collection\n" 
       print $r->get_resourcelist->as_string . "\n"
    } else {
       print $r->get_property("getcontentlength") ."\n";
    }
 }

Please note that although you may set a different namespace for a property of a resource during a set_prop, HTTP::DAV currently ignores all XML namespaces so you will get clashes if two properties have the same name but in different namespaces. Currently this is unavoidable but I'm working on the solution.

=item B<proppatch([URL],[NAMESPACE],PROPNAME,PROPVALUE,ACTION,[NSABBR])>

If C<-action> equals "set" then we set a property named C<-propname> to C<-propvalue> in the namespace C<-namespace> for C<-url>. 

If C<-action> equals "remove" then we unset a property named C<-propname> in the namespace C<-namespace> for C<-url>. 

If no action is supplied then the default action is "set".

The return value is an C<HTTP::DAV::Resource> object on success or 0 on failure.

The Resource object can be used for interrogating properties or performing other operations.

To explicitly set a namespace in which to set the propname then you can use the C<-namespace> and C<-nsabbr> (namespace abbreviation) parameters. But you're welcome to play around with DAV namespaces.

Requires a working resource to be set before being called. See C<open>.

It is recommended that you use C<set_prop> and C<unset_prop> instead of proppatch for readability. 

C<set_prop> simply calls C<proppatch(-action=>set)> and C<unset_prop> calls C<proppatch(-action=>"remove")>

See C<set_prop> and C<unset_prop> for examples.

=item B<put(LOCAL,[URL],[CALLBACK],[HEADERS])>

uploads the files or directories at C<-local> to the remote destination at C<-url>.

C<-local> points to a file, directory or series of files or directories (indicated by a glob).

If the filename contains any of the characters `*',  `?' or  `['  it is a candidate for filename substitution, also  known  as  ``globbing''.   This word  is  then regarded as a pattern (``glob-pattern''), and replaced with an alphabetically sorted list  of  file  names which match the pattern.  

One can upload/put a string by passing a reference to a scalar in the -local parameter. See example below.

put requires a working resource to be set before being called. See C<open>.

The return value is always 1 or 0 indicating success or failure.

See L<get()> for a description of what the optional callback parameter does.

You can also pass a C<-headers> argument. That allows to specify custom HTTP headers. It can be either a hashref with header names and values, or a L<HTTP::Headers> object.

B<put examples:>

Put a string to the server:

  my $myfile = "This is the contents of a file to be uploaded\n";
  $d->put(-local=>\$myfile,-url=>"http://www.host.org/dav_dir/file.txt");

Put a local file to the server:

  $d->put(-local=>"/tmp/index.html",-url=>"http://www.host.org/dav_dir/");

Put a series of local files to the server:

  In these examples, /tmp contains file1.html, file1, file2.html, 
  file2.txt, file3.html, file2/

  $d->put(-local=>"/tmp/file[12]*",-url=>"http://www.host.org/dav_dir/");
  
  uploads file1.html, file1, file2.html, file2.txt and the directory file2/ to dav_dir/.

=item B<set_prop([URL],[NAMESPACE],PROPNAME,PROPVALUE)>

Sets a property named C<-propname> to C<-propvalue> in the namespace C<-namespace> for C<-url>. 

Requires a working resource to be set before being called. See C<open>.

The return value is an C<HTTP::DAV::Resource> object on success or 0 on failure.

The Resource object can be used for interrogating properties or performing other operations.

Example:

 if ( $r = $d->set_prop(-url=>$url,
              -namespace=>"dave",
              -propname=>"author",
              -propvalue=>"Patrick Collins"
             ) ) {
    print "Author property set\n";
 } else {
    print "set_prop failed:" . $d->message . "\n";
 }

See the note in propfind about namespace support in HTTP::DAV. They're settable, but not readable.



=item B<steal([URL])>

forcefully steals any locks held against URL.

steal will perform a propfind against URL and then, any locks that are found will be unlocked one by one regardless of whether we own them or not.

Requires a working resource to be set before being called. See C<open>.

The return value is always 1 or 0 indicating success or failure. If multiple locks are found and unlocking one of them fails then the operation will be aborted.

 if ($d->steal()) {
    print "Steal succeeded\n";
 } else {
    print "Steal failed: ". $d->message() . "\n";
 }

=item B<unlock([URL])>

unlocks any of our locks on URL.

Requires a working resource to be set before being called. See C<open>.

The return value is always 1 or 0 indicating success or failure.

 if ($d->unlock()) {
    print "Unlock succeeded\n";
 } else {
    print "Unlock failed: ". $d->message() . "\n";
 }

=item B<unset_prop([URL],[NAMESPACE],PROPNAME)>

Unsets a property named C<-propname> in the namespace C<-namespace> for C<-url>. 
Requires a working resource to be set before being called. See C<open>.

The return value is an C<HTTP::DAV::Resource> object on success or 0 on failure.

The Resource object can be used for interrogating properties or performing other operations.

Example:

 if ( $r = $d->unset_prop(-url=>$url,
              -namespace=>"dave",
              -propname=>"author",
             ) ) {
    print "Author property was unset\n";
 } else {
    print "set_prop failed:" . $d->message . "\n";
 }

See the note in propfind about namespace support in HTTP::DAV. They're settable, but not readable.

=back

=head2 ACCESSOR METHODS

=over 4 

=item B<get_user_agent>

Returns the clients' working C<HTTP::DAV::UserAgent> object. 

You may want to interact with the C<HTTP::DAV::UserAgent> object 
to modify request headers or provide advanced authentication 
procedures. See dave for an advanced authentication procedure.

=item B<get_last_request>

Takes no arguments and returns the clients' last outgoing C<HTTP::Request> object. 

You would only use this to inspect a request that has already occurred.

If you would like to modify the C<HTTP::Request> BEFORE the HTTP request takes place (for instance to add another header), you will need to get the C<HTTP::DAV::UserAgent> using C<get_user_agent> and interact with that.

=item B<get_workingresource>

Returns the currently "opened" or "working" resource (C<HTTP::DAV::Resource>).

The working resource is changed whenever you open a url or use the cwd command.

e.g. 
  $r = $d->get_workingresource
  print "pwd: " . $r->get_uri . "\n";

=item B<get_workingurl>

Returns the currently "opened" or "working" C<URL>.

The working resource is changed whenever you open a url or use the cwd command.

  print "pwd: " . $d->get_workingurl . "\n";

=item B<get_lockedresourcelist>

Returns an C<HTTP::DAV::ResourceList> object that represents all of the locks we've created using THIS dav client.

  print "pwd: " . $d->get_workingurl . "\n";

=item B<get_absolute_uri(REL_URI,[BASE_URI])>

This is a useful utility function which joins C<BASE_URI> and C<REL_URI> and returns a new URI.

If C<BASE_URI> is not supplied then the current working resource (as indicated by get_workingurl) is used. If C<BASE_URI> is not set and there is no current working resource the C<REL_URI> will be returned.

For instance:
 $d->open("http://host.org/webdav/dir1/");

 # Returns "http://host.org/webdav/dir2/"
 $d->get_absolute_uri(-rel_uri=>"../dir2");

 # Returns "http://x.org/dav/dir2/file.txt"
 $d->get_absolute_uri(-rel_uri  =>"dir2/file.txt",
                      ->base_uri=>"http://x.org/dav/");

Note that it subtly takes care of trailing slashes.

=back

=head2 ERROR HANDLING METHODS

=over 4

=item B<message>

C<message> gets the last success or error message.

The return value is always a scalar (string) and will change everytime a dav operation is invoked (lock, cwd, put, etc).

See also C<errors> for operations which contain multiple error messages.

=item B<errors>

Returns an @array of error messages that had been set during a multi-request operation.

Some of C<HTTP::DAV>'s operations perform multiple request to the server. At the time of writing only put and get are considered multi-request since they can operate recursively requiring many HTTP requests. 

In these situations you should check the errors array if to determine if any of the requests failed.

The C<errors> function is used for multi-request operations and not to be confused with a multi-status server response. A multi-status server response is when the server responds with multiple error messages for a SINGLE request. To deal with multi-status responses, see C<HTTP::DAV::Response>.

 # Recursive put
 if (!$d->put( "/tmp/my_dir", $url ) ) {
    # Get the overall message
    print $d->message;
    # Get the individual messages
    foreach $err ( $d->errors ) { print "  Error:$err\n" }
 }

=item B<is_success>

Returns the status of the last DAV operation performed through the HTTP::DAV interface.

This value will always be the same as the value returned from an HTTP::DAV::method. For instance:

  # This will always evaluate to true
  ($d->lock($url) eq $d->is_success) ?

You may want to use the is_success method if you didn't capture the return value immediately. But in most circumstances you're better off just evaluating as follows:
  if($d->lock($url)) { ... }

=item B<get_last_response>

Takes no arguments and returns the last seen C<HTTP::DAV::Response> object. 

You may want to use this if you have just called a propfind and need the individual error messages returned in a MultiStatus.

If you find that you're using get_last_response() method a lot, you may be better off using the more advanced C<HTTP::DAV> interface and interacting with the HTTP::DAV::* interfaces directly as discussed in the intro. For instance, if you find that you're always wanting a detailed understanding of the server's response headers or messages, then you're probably better off using the C<HTTP::DAV::Resource> methods and interpreting the C<HTTP::DAV::Response> directly.

To perform detailed analysis of the server's response (if for instance you got back a multistatus response) you can call C<get_last_response()> which will return to you the most recent response object (always the result of the last operation, PUT, PROPFIND, etc). With the returned HTTP::DAV::Response object you can handle multi-status responses.

For example:

   # Print all of the messages in a multistatus response
   if (! $d->unlock($url) ) {
      $response = $d->get_last_response();
      if ($response->is_multistatus() ) {
        foreach $num ( 0 .. $response->response_count() ) {
           ($err_code,$mesg,$url,$desc) =
              $response->response_bynum($num);
           print "$mesg ($err_code) for $url\n";
        }
      }
   }

=back

=head2 ADVANCED METHODS

=over 4

=item B<new_resource>

Creates a new resource object with which to play.
This is the preferred way of creating an C<HTTP::DAV::Resource> object if required.
Why? Because each Resource object needs to sit within a global HTTP::DAV client. 
Also, because the new_resource routine checks the C<HTTP::DAV> locked resource
list before creating a new object.

    $dav->new_resource( -uri => "http://..." );

=item B<set_workingresource(URL)>

Sets the current working resource to URL.

You shouldn't need this method. Call open or cwd to set the working resource.

You CAN call C<set_workingresource()> but you will need to perform a
C<propfind> immediately following it to ensure that the working
resource is valid.

=back

=head1 INSTALLATION, TODO, MAILING LISTS and REVISION HISTORY

[OUTDATED]

Please see the primary HTTP::DAV webpage at
(http://www.webdav.org/perldav/http-dav/)
or the README file in this library.

=head1 SEE ALSO

You'll want to also read:

=over *

=item C<HTTP::DAV::Response>

=item C<HTTP::DAV::Resource>

=item C<dave>

=back

and maybe if you're more inquisitive:

=over *

=item C<LWP::UserAgent>

=item C<HTTP::Request>

=item C<HTTP::DAV::Comms>

=item C<HTTP::DAV::Lock>

=item C<HTTP::DAV::ResourceList>

=item C<HTTP::DAV::Utils>

=back

=head1 AUTHOR AND COPYRIGHT

This module is Copyright (C) 2001-2008 by

    Patrick Collins
    G03 Gloucester Place, Kensington
    Sydney, Australia

    Email: pcollins@cpan.org
    Phone: +61 2 9663 4916

All rights reserved.

Current co-maintainer of the module is Cosimo Streppone
for Opera Software ASA, L<opera@cpan.org>.

You may distribute this module under the terms of either the
GNU General Public License or the Artistic License,
as specified in the Perl README file.

=cut

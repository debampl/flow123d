"""
Contains all ModelEditor notifications and their codes.
"""

__author__ = 'Tomas Krizek'


_NOTIFICATIONS = [

    # =====================================================
    #                  FATAL ERRORS
    # =====================================================

    {
        'code': 100,
        'name': 'FatalError',
        'description': 'Generic Fatal Error',
        'message': '{0}',
        'example': '',
    },

    # -----------------------------------------------------
    #                Syntax Fatal Errors
    # -----------------------------------------------------

    {
        'code': 101,
        'name': 'SyntaxFatalError',
        'description': 'Occurs when pyyaml can not parse the input.',
        'message': 'Unable to parse remaining input',
        'example': 'example_invalid_syntax.yaml'
    },
    {
        'code': 102,
        'name': 'ComplexRecordKey',
        'description': 'When user attempts to use an array or record as a key.',
        'message': 'Only scalar keys are supported for records',
        'example': 'example_complex_record_key.yaml'
    },

    # =====================================================
    #                        ERRORS
    # =====================================================

    {
        'code': 300,
        'name': 'Error',
        'description': 'Generic Error',
        'message': '{0}',
        'example': '',
    },

    # -----------------------------------------------------
    #                  Validation Errors
    # -----------------------------------------------------

    {
        'code': 301,
        'name': 'ValidationError',
        'description': 'Occurs during data validation. More specific error should be used when '
                       'possible.',
        'message': '{0}',
        'example': ''
    },
    {
        'code': 302,
        'name': 'ValidationTypeError',
        'description': 'Validation expects a different type and autoconversion can not resolve the '
                       'correct type.',
        'message': 'Expected type {0}',
        'example': 'example_validation_type_error.yaml',
    },
    {
        'code': 303,
        'name': 'ValueTooSmall',
        'description': 'Value is smaller than specified minimum.',
        'message': 'Value has to be larger or equal to {0}',
        'example': 'example_validation_scalar.yaml',
    },
    {
        'code': 304,
        'name': 'ValueTooBig',
        'description': 'Value is larger than specified maximum',
        'message': 'Value has to be smaller or equal to {0}',
        'example': 'example_validation_scalar.yaml',
    },
    {
        'code': 305,
        'name': 'InvalidSelectionOption',
        'description': 'When validating Selection, the given option does not exist.',
        'message': '{0} has not option {1}',
        'example': 'example_validation_scalar.yaml',
    },
    {
        'code': 306,
        'name': 'NotEnoughItems',
        'description': 'Array is smaller than its specified minimum size.',
        'message': 'Array has to have at least {0} item(s)',
        'example': 'example_validation_array.yaml',
    },
    {
        'code': 307,
        'name': 'TooManyItems',
        'description': 'Array is larger than its specified maximum size.',
        'message': 'Array cannot have more than {0} items(s)',
        'example': 'example_validation_array.yaml',
    },
    {
        'code': 308,
        'name': 'MissingObligatoryKey',
        'description': 'Record is missing an obligatory key.',
        'message': 'Missing obligatory key "{0}" in record {1}',
        'example': 'example_validation_record.yaml',
    },
    {
        'code': 309,
        'name': 'MissingAbstractRecordType',
        'description': 'AbstractRecord type is missing and has no default_descendant.',
        'message': 'Missing record type',
        'example': 'example_validation_abstract_record.yaml',
    },
    {
        'code': 310,
        'name': 'InvalidAbstractRecordType',
        'description': 'AbstractRecord type is invalid.',
        'message': 'Invalid TYPE "{0}" for record {1}',
        'example': 'example_validation_abstract_record.yaml',
    },

    # -----------------------------------------------------
    #                  Format Errors
    # -----------------------------------------------------

    {
        'code': 400,
        'name': 'FormatError',
        'description': 'Occurs when format file is invalid. More specific warning should be used '
                       'when possible.',
        'message': '{0}',
        'example': ''
    },
    {
        'code': 401,
        'name': 'InputTypeNotSupported',
        'description': 'Format contains an input type that is not supported.',
        'message': 'Unexpected input_type "{0}" in format',
        'example': '',
    },

    # -----------------------------------------------------
    #                  Syntax Errors
    # -----------------------------------------------------

    {
        'code': 500,
        'name': 'SyntaxError',
        'description': 'Occurs when YAML syntax is invalid. More specific error should be used '
                       'when possible.',
        'message': '{0}',
        'example': ''
    },
    {
        'code': 501,
        'name': 'UndefinedAnchor',
        'description': 'The anchor is not yet defined.',
        'message': 'Anchor "&{0}" is not defined',
        'example': 'example_anchors.yaml',
    },
    # 502 deleted
    {
        'code': 503,
        'name': 'NoReferenceToMerge',
        'description': 'When user specifies invalid content for the merge key. Only a reference or'
                       'an array of references are accepted.',
        'message': 'Merge key does not contain a reference neither an array of references.',
        'example': 'example_merge.yaml'
    },
    {
        'code': 504,
        'name': 'InvalidReferenceForMerge',
        'description': 'User references a node to be merged. Error occurs if that node is not a'
                       'Record.',
        'message': 'Reference node "*{0}" is can not be merged (not a Record)',
        'example': 'example_merge.yaml'
    },
    {
        'code': 505,
        'name': 'ConstructScalarError',
        'description': 'Scalar value can not be constructed from the input.',
        'message': '{0}',
        'example': 'example_construct_scalar.yaml'
    },


    # =====================================================
    #                        WARNINGS
    # =====================================================

    {
        'code': 600,
        'name': 'Warning',
        'description': 'Generic Warning',
        'message': '{0}',
        'example': '',
    },

    # -----------------------------------------------------
    #                  Validation Warnings
    # -----------------------------------------------------

    {
        'code': 601,
        'name': 'ValidationWarning',
        'description': 'Occurs during data validation. More specific warning should be used when '
                       'possible.',
        'message': '{0}',
        'example': ''
    },
    {
        'code': 602,
        'name': 'UnknownRecordKey',
        'description': 'Unspecified key is encountered in Record.',
        'message': 'Unknown key "{0}" in record {1}',
        'example': 'example_validation_record.yaml',
    },

    # =====================================================
    #                   INFO MESSAGES
    # =====================================================

    {
        'code': 800,
        'name': 'Info',
        'description': 'Generic Info',
        'message': '{0}',
        'example': '',
    },

    # -----------------------------------------------------
    #                  Validation Info
    # -----------------------------------------------------

    {
        'code': 801,
        'name': 'ValidationInfo',
        'description': 'Occurs during data validation. More specific info should be used when '
                       'possible.',
        'message': '{0}',
        'example': ''
    },

    # -----------------------------------------------------
    #                  Syntax Info
    # -----------------------------------------------------

    {
        'code': 900,
        'name': 'SyntaxInfo',
        'description': 'Occurs during syntax parsing. More specific info should be used when '
                       'possible.',
        'message': '{0}',
        'example': ''
    },
    {
        'code': 901,
        'name': 'UselessTag',
        'description': 'When tag is found anywhere except AbstractRecord.',
        'message': 'Tag "!{0}" has no effect here',
        'example': None,
        'deprecated': True
        # This info can no longer happen. The combination of autoconversions and abstract records
        # makes this error very hard to detect.
    },
    {
        'code': 902,
        'name': 'OverridingAnchor',
        'description': 'When an anchor with the same name was defined previously.',
        'message': 'Previously defined anchor "&{0}" overwritten',
        'example': 'example_anchors.yaml'
    },
    {
        'code': 903,
        'name': 'MultiLineFlow',
        'description': 'Flow style is used across multiple lines.',
        'message': 'Using flow style across multiple lines is not recommended',
        'example': 'example_flow_style.yaml'
    },
]


NOTIFICATIONS_BY_CODE = {notification['code']: notification for notification in _NOTIFICATIONS}
NOTIFICATIONS_BY_NAME = {notification['name']: notification for notification in _NOTIFICATIONS}
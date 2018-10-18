using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class newsViewModel
    {
        public long id { get; set; }
        public string msg { get; set; }
        public DateTime? add_ts { get; set; }
        public DateTime? edit_ts { get; set; }
        public int lang { get; set; }
        public long uid { get; set; }
    }
}
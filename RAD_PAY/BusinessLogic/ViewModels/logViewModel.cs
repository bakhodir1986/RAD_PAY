using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class logViewModel
    {
        public long id { get; set; }
        public int? aid { get; set; }
        public DateTime? add_ts { get; set; }
        public string action { get; set; }
    }
}